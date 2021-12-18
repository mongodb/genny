// Copyright 2021-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cast_core/actors/GetMoreActor.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {

//
// The `PhaseLoop<PhaseConfig>` type constructs one `PhaseConfig` instance
// for each `Phase:` block in your Actor's YAML. We do this at
// Actor/Workload setup time and before we start recording real metrics.
// This lets you do any complicated or costly per-Phase setup operations
// (such as interacting with YAML or doing syntax validations) without
// impacting your metrics.
//
// Imagine you have the following YAML:
//
//     SchemaVersion: 2017-07-01
//     Actors:
//     - Name: GetMoreActor
//       Type: GetMoreActor
//       Threads: 100
//       Database: test
//       Phases:
//       - Collection: forPhase0
//         Document: {a: 7}
//         Duration: 1 minute
//       - Collection: forPhase1
//         Document: {b: 7}
//         Repeat: 100
//
// We'll automatically construct 2 `PhaseConfig`s.
//
// You can pass additional parameters to your `PhaseConfig` type by adding
// them to the `_loop{}` initializer in the `GetMoreActor` constructor
// below. The first constructor parameter must always be a `PhaseContext&`
// which lets you access the per-Phase configuration. In this example, the
// constructor also requires a `database&` and the `ActorId` which we pass
// along in the initializer.
//
// For more information on the Duration and Repeat keywords and how multiple Actors
// coordinate across Phases, see the extended example workload
// `src/workloads/docs/HelloWorld-MultiplePhases.yml`.
//
// (`WorkloadContext`, `ActorContext`, and `PhaseContext` are all defined
// in `context.hpp` - see full documentation there.)
//
// Within your Actor's `run()` method, defined below, we iterate over the
// `PhaseLoop<PhaseConfig> _loop` variable, and you can access the
// `PhaseConfig` instance constructed for the current Phase via the
// `config` variable using `->`:
//
//     for (auto&& config : _loop) {
//         for (const auto&& _ : config) {
//             // this will be forPhase0 during Phase 0
//             // and forPhase1 during Phase 1
//             auto collection = config->collection;
//         }
//     }
//
// The inner loop `for(const auto&& _ : config)` is how PhaseLoop keeps
// running your code during the course of the current Phase. In the above
// example, `PhaseLoop` will see that Phase 0 is supposed to run for 1
// minute so it will continue running the body of the inner loop for 1
// minute before it will signal to the Genny internals that it is done with
// the current Phase. Once all Actors indicate that they are done with the
// current Phase, Genny lets Actors proceed with the next Phase by
// advancing to the next `config` instance in the outer loop.
//
// See the full documentation in `PhaseLoop.hpp`.
//

struct GetMoreActor::PhaseConfig {
    mongocxx::database database;

    mongocxx::collection collection;

    //
    // DocumentGenerator is a powerful mini-templating-engine that lets you
    // generate random data at runtime using a simple templating language. The
    // best way to learn it is to look at a few examples in the `src/workloads`
    // directory. Here's a simple example:
    //
    //     #...
    //     Phases:
    //     - Phase: 0
    //       Document: {a: {^RandomInt: {min: 0, max: 100}}}
    //
    // When we construct a `DocumentGenerator` from this:
    //
    //     auto docGen = phaseContext["Document"].to<DocumentGenerator>(phaseContext, actorId);
    //
    // We can evaluate it multiple times to get a randomly-generated document:
    //
    //     bsoncxx::document::value first = docGen();  // => {a: 27}
    //     bsoncxx::document::value second = docGen(); // => {a: 34}
    //
    // All document-generators are deterministically seeded so all runs of the
    // same workload produce the same set of documents.
    //
    DocumentGenerator documentExpr;


    //
    // When Genny constructs each Actor it assigns it a unique ID. This is used
    // by many of the `PhaseContext` and `ActorContext` methods. This is becase
    // we construct only one `ActorContext` for the entire `Actor:` block even
    // if we're constructing 100 instances of the same Actor type. See the full
    // documentation in `context.hpp`.
    //

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db},
          collection{database[phaseContext["Collection"].to<std::string>()]},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}
};


//
// Genny spins up a thread for each Actor instance. The `Threads:` configuration
// tells Genny how many such instances to create. See further documentation in
// the `Actor.hpp` file.
//
void GetMoreActor::run() {
    //
    // The `config` variable is bound to the `PhaseConfig` that was
    // constructed for the current Phase at setup time. This loop will automatically
    // block, wait, and repeat at the right times to gracefully coordinate with other
    // Actors so that all Actors start and end at the right time.
    //
    for (auto&& config : _loop) {
        //
        // This inner loop is run according to the Phase configuration for this
        // Actor. If you have `{Duration: 1 minute}` this loop will be run
        // for one minute, etc. It also handles rate-limiting and error-handling
        // as discussed below.
        //
        // The body of the below "inner" loop is where most of your logic should start.
        // Alternatively you may decide to put some logic in your `PhaseConfig` type
        // similarly to what the `CrudActor` does. If your Actor is simple and
        // only supports a single type of operation, it is easiest to put your logic in
        // the body of the below loop.
        //
        for (const auto&& _ : config) {
            //
            // Evaluate the DocumentGenerator template:
            //
            auto document = config->documentExpr();

            //
            // The clock starts running only when you call `.start()`. The returned object
            // from this lets you record how many bytes, documents, and actual
            // iterations (usually 1) your Actor completes while the clock is running. You then
            // must stop the clock by calling either `.success()` if everything is okay
            // or `.fail()` if there was a problem. It's undefined behavior what will
            // happen if you don't stop the metrics clock (e.g. if there is an uncaught
            // exception). See the full documentation on `OperationContext`.
            //
            // We don't care about how long the value-generator takes to run
            // so we don't start the clock until after evaluating it. Purists
            // would argue we even start until after we've done the below log
            // statement. You decide.
            //
            auto inserts = _totalInserts.start();

            //
            // You have the full power of boost logging which is rendered to stdout.
            // Convention is to log generated documents and "normal" events at the
            // debug level.
            //
            BOOST_LOG_TRIVIAL(debug) << " GetMoreActor Inserting "
                                    << bsoncxx::to_json(document.view());

            //
            // ⚠️ If your Actor throws any uncaught exceptions, the whole Workload will
            // attempt to end as quickly as possible. Every time this inner loop loops
            // around, it checks for any other Actors' exceptions and will stop iterating
            // if it sees any. Such failed workloads are considered "programmer error"
            // and will mark the workload task as a system-failure in Evergreen. ⚠️
            //

            //
            // Actually do the logic.
            //
            // We expect that this insert operation may actually fail sometimes and we
            // wouldn't consider this to be programmer-error (or a configuration-
            // error) so we catch try/catch with the expected exception types.
            //
            try {

                config->collection.insert_one(document.view());

                inserts.addDocuments(1);
                inserts.addBytes(document.view().length());
                inserts.success();
            } catch(mongocxx::operation_exception& e) {
                inserts.failure();
                //
                // MongoException lets you include a "causing" bson document in the
                // exception message for help debugging.
                //
                BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            }
        }
    }
}

GetMoreActor::GetMoreActor(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", GetMoreActor::id())},
      _client{context.client()},
      //
      // Pass any additional constructor parameters that your `PhaseConfig` needs.
      //
      // The first argument passed in here is actually the `ActorContext` but the
      // `PhaseLoop` reads the `PhaseContext`s from there and constructs one
      // instance for each Phase.
      //
      _loop{context, (*_client)[context["Database"].to<std::string>()], GetMoreActor::id()} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
auto registerGetMoreActor = Cast::registerDefault<GetMoreActor>();
}  // namespace
}  // namespace genny::actor
