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

#include <cast_core/actors/CollectionSharder.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

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

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

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
//     - Name: CollectionSharder
//       Type: CollectionSharder
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
// them to the `_loop{}` initializer in the `CollectionSharder` constructor
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

class ShardCollectionOperation {
    public:
    ShardCollectionOperation(const Node& node, PhaseContext& phaseContext, ActorId id) :           
        _databaseName{node["Database"].to<std::string>()},
        _collectionName{node["Collection"].to<std::string>()},
        _keyExpr{node["key"].to<DocumentGenerator>(phaseContext, id)},
        _unique{node["unique"].maybe<bool>()},
        _numInitialChunks{node["numInitialChunks"].maybe<IntegerSpec>()},
        _presplitHashedZones{node["presplitHashedZones"].maybe<bool>()},
        _collation{node["collation"].maybe<DocumentGenerator>(phaseContext, id)},
        _enableShardingCommand{make_document(kvp("enableSharding", _databaseName))},
        _shardCollectionCommand{make_document()}
    {
        std::string ns = _databaseName + "." + _collectionName;
        bsoncxx::builder::stream::document documentBuilder{};
        documentBuilder << "shardCollection" << ns << "key" << _keyExpr();

        if(_unique) {
            documentBuilder << "unique" << *_unique;
        }
        if(_numInitialChunks) {
            documentBuilder << "numInitialChunks" << *_numInitialChunks;
        }
        if(_presplitHashedZones) {
            documentBuilder << "presplitHashedZones" << *_presplitHashedZones;
        }
        if(_collation) {
            documentBuilder << "collation" << _collation->evaluate();
        }
        _shardCollectionCommand = documentBuilder.extract();
    }

    void run(mongocxx::database& adminDatabase) {
        BOOST_LOG_TRIVIAL(info) << "Sharding " << _databaseName << "." << _collectionName;
        adminDatabase.run_command(_enableShardingCommand.view());
        adminDatabase.run_command(_shardCollectionCommand.view());
    }

    auto getCommand() const {
        return _shardCollectionCommand.view();
    }

    private:
    std::string _databaseName;
    std::string _collectionName;
    DocumentGenerator _keyExpr;
    std::optional<bool> _unique;
    std::optional<int64_t> _numInitialChunks;
    std::optional<bool> _presplitHashedZones;
    std::optional<DocumentGenerator> _collation;
    bsoncxx::document::value _enableShardingCommand;
    bsoncxx::document::value _shardCollectionCommand;

};

struct CollectionSharder::PhaseConfig {
    mongocxx::database adminDatabase;
    std::vector<std::unique_ptr<ShardCollectionOperation>> operations;

    //
    // When Genny constructs each Actor it assigns it a unique ID. This is used
    // by many of the `PhaseContext` and `ActorContext` methods. This is becase
    // we construct only one `ActorContext` for the entire `Actor:` block even
    // if we're constructing 100 instances of the same Actor type. See the full
    // documentation in `context.hpp`.
    //

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& adminDb, ActorId id)
        : adminDatabase{std::move(adminDb)}
        {
            auto createOperation = [&](const Node& node) {
                return std::make_unique<ShardCollectionOperation>(node, phaseContext, id);
            };
            operations = phaseContext.getPlural<std::unique_ptr<ShardCollectionOperation>>(
            "ShardCollection", "ShardCollections", createOperation);
        }

    bool isMongos() {
        auto helloResult = adminDatabase.run_command(make_document(kvp("hello", 1)));
        if(auto msg = helloResult.view()["msg"]) {
            return true;
        }
        return false;
    }
};


//
// Genny spins up a thread for each Actor instance. The `Threads:` configuration
// tells Genny how many such instances to create. See further documentation in
// the `Actor.hpp` file.
//
void CollectionSharder::run() {
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

            if(config->isMongos()) {
                auto metrics = _shardCollectionMetrics.start();
                for(auto& op : config->operations) {
                    try {
                        op->run(config->adminDatabase);
                    } catch(mongocxx::operation_exception& e) {
                        metrics.failure();
                        //
                        // MongoException lets you include a "causing" bson document in the
                        // exception message for help debugging.
                        //
                        BOOST_THROW_EXCEPTION(MongoException(e, op->getCommand()));
                    }
                }
                metrics.success();
            }
        }
    }
}

CollectionSharder::CollectionSharder(genny::ActorContext& context)
    : Actor{context},
      _shardCollectionMetrics{context.operation("ShardCollection", CollectionSharder::id())},
      _client{context.client()},
      //
      // Pass any additional constructor parameters that your `PhaseConfig` needs.
      //
      // The first argument passed in here is actually the `ActorContext` but the
      // `PhaseLoop` reads the `PhaseContext`s from there and constructs one
      // instance for each Phase.
      //
      _loop{context, (*_client)["admin"], CollectionSharder::id()} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
auto registerCollectionSharder = Cast::registerDefault<CollectionSharder>();
}  // namespace
}  // namespace genny::actor
