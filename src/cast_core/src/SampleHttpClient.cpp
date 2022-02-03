// Copyright 2022-present MongoDB Inc.
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

#include <cast_core/actors/SampleHttpClient.hpp>

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
#include <simple-beast-client/httpclient.hpp>


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
//     - Name: SampleHttpClient
//       Type: SampleHttpClient
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
// them to the `_loop{}` initializer in the `SampleHttpClient` constructor
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

struct SampleHttpClient::PhaseConfig {
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
void SampleHttpClient::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto document = config->documentExpr();

            auto inserts = _totalInserts.start();

            BOOST_LOG_TRIVIAL(debug) << " SampleHttpClient Inserting "
                                    << bsoncxx::to_json(document.view());

            try {
                boost::asio::io_context ioContext;
                auto client = std::make_shared<simple_http::get_client>(
                    ioContext,
                    [](simple_http::empty_body_request& /*req*/,
                    simple_http::string_body_response& resp) {
                        // Display the response to the console.
                    });

                client->setFailHandler([&inserts](const simple_http::empty_body_request& /*req*/,
                            const simple_http::string_body_response& /*resp*/,
                            simple_http::fail_reason fr, boost::string_view message) {
                        std::cout << fr << '\n';
                    inserts.failure();
                });

                // Run the GET request to httpbin.org
                client->get(simple_http::url{
                    "https://google.com"});

                ioContext.run();

                inserts.success();
            } catch(mongocxx::operation_exception& e) {
                inserts.failure();
                //
                // MongoException lets you include a "causing" bson document in the
                // exception message for help debugging.
                //
                BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            } catch(...) {
                inserts.failure();
                throw std::current_exception();
            }
        }
    }
}

SampleHttpClient::SampleHttpClient(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", SampleHttpClient::id())},
      _client{context.client()},
      //
      // Pass any additional constructor parameters that your `PhaseConfig` needs.
      //
      // The first argument passed in here is actually the `ActorContext` but the
      // `PhaseLoop` reads the `PhaseContext`s from there and constructs one
      // instance for each Phase.
      //
      _loop{context, (*_client)[context["Database"].to<std::string>()], SampleHttpClient::id()} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
auto registerSampleHttpClient = Cast::registerDefault<SampleHttpClient>();
}  // namespace
}  // namespace genny::actor
