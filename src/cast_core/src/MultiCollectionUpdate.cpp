// Copyright 2019-present MongoDB Inc.
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

#include <cast_core/actors/MultiCollectionUpdate.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/** @private */
struct MultiCollectionUpdate::PhaseConfig {
    PhaseConfig(PhaseContext& context, mongocxx::pool::entry& client, ActorId id)
        : database{(*client)[context["Database"].to<std::string>()]},
          numCollections{context["CollectionCount"].to<IntegerSpec>()},
          queryExpr{context["UpdateFilter"].to<DocumentGenerator>(context, id)},
          updateExpr{context["Update"].to<DocumentGenerator>(context, id)},
          uniformDistribution{0, numCollections} {}

    mongocxx::database database;
    int64_t numCollections;
    DocumentGenerator queryExpr;
    DocumentGenerator updateExpr;
    // TODO: Enable passing in update options.
    //    DocumentGenerator  updateOptionsExpr;

    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<int64_t> uniformDistribution;
};

void MultiCollectionUpdate::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            // Select a collection
            auto collectionNumber = config->uniformDistribution(_rng);
            auto collectionName = "Collection" + std::to_string(collectionNumber);
            auto collection = config->database[collectionName];

            // Perform update
            auto filter = config->queryExpr();
            auto update = config->updateExpr();
            // BOOST_LOG_TRIVIAL(info) << "Filter is " <<  bsoncxx::to_json(filter.view());
            // BOOST_LOG_TRIVIAL(info) << "Update is " << bsoncxx::to_json(update.view());
            // BOOST_LOG_TRIVIAL(info) << "Collection Name is " << collectionName;
            {
                // Only time the actual update, not the setup of arguments
                auto opCtx = _updateOp.start();
                auto result = collection.update_many(std::move(filter), std::move(update));
                opCtx.addDocuments(result->modified_count());
                opCtx.success();
            }
        }
    }
}

MultiCollectionUpdate::MultiCollectionUpdate(genny::ActorContext& context)
    : Actor(context),
      _updateOp{context.operation("Update", MultiCollectionUpdate::id())},
      _client{std::move(context.client())},
      _loop{context, _client, MultiCollectionUpdate::id()} {}

namespace {
auto registerMultiCollectionUpdate =
    genny::Cast::registerDefault<genny::actor::MultiCollectionUpdate>();
}
}  // namespace genny::actor
