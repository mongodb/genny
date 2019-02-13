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

#include <cast_core/actors/MultiCollectionQuery.hpp>

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

#include <value_generators/value_generators.hpp>

namespace genny::actor {

/** @private */
struct MultiCollectionQuery::PhaseConfig {
    PhaseConfig(PhaseContext& context, mongocxx::pool::entry& client)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<IntegerSpec, true>("CollectionCount")},
          filterExpr{value_generators::Expression::parseOperand(context.get("Filter"))},
          uniformDistribution{0, numCollections} {}

    mongocxx::database database;
    int64_t numCollections;
    value_generators::UniqueExpression filterExpr;

    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<int64_t> uniformDistribution;
    mongocxx::options::find options;
};

void MultiCollectionQuery::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            // Select a collection
            // This area is ripe for defining a collection generator, based off a string generator.
            // It could look like: collection: {^Concat: [Collection, ^RandomInt: {min: 0, max:
            // *CollectionCount]} Requires a string concat generator, and a translation of a string
            // to a collection
            auto collectionNumber = config->uniformDistribution(_rng);
            auto collectionName = "Collection" + std::to_string(collectionNumber);
            auto collection = config->database[collectionName];

            // Perform a query
            auto filter = config->filterExpr->evaluate(_rng).getDocument();
            // BOOST_LOG_TRIVIAL(info) << "Filter is " <<  bsoncxx::to_json(filter.view());
            // BOOST_LOG_TRIVIAL(info) << "Collection Name is " << collectionName;
            {
                // Only time the actual update, not the setup of arguments
                auto opCtx = _queryOp.start();
                auto cursor = collection.find(std::move(filter), config->options);
                // exhaust the cursor
                uint count = 0;
                for (auto&& doc : cursor) {
                    doc.length();
                    count++;
                }
                opCtx.addOps(count);
                opCtx.success();
            }
        }
    }
}

MultiCollectionQuery::MultiCollectionQuery(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _queryOp{context.operation("Query", MultiCollectionQuery::id())},
      _client{std::move(context.client())},
      _loop{context, _client} {}

namespace {
auto registerMultiCollectionQuery =
    genny::Cast::registerDefault<genny::actor::MultiCollectionQuery>();
}
}  // namespace genny::actor
