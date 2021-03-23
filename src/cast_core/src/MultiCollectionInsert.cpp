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

#include <cast_core/actors/MultiCollectionInsert.hpp>
#include <cast_core/actors/OptionsConversion.hpp>

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
struct MultiCollectionInsert::PhaseConfig {
    PhaseConfig(PhaseContext& context, mongocxx::pool::entry& client, ActorId id)
        : database{(*client)[context["Database"].to<std::string>()]},
          numCollections{context["CollectionCount"].to<IntegerSpec>()},
          batchSize{context["BatchSize"].maybe<IntegerSpec>().value_or(0)},
          docExpr{context["Document"].to<DocumentGenerator>(context, id)},
          uniformDistribution{0, numCollections - 1},
          insertOperation{context.operation("Insert", id)},
          insertOptions{context["OperationOptions"].maybe<mongocxx::options::insert>().value_or(
              mongocxx::options::insert{})} {}

    mongocxx::database database;
    int64_t numCollections;
    int64_t batchSize;
    DocumentGenerator docExpr;
    mongocxx::options::insert insertOptions;

    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<int64_t> uniformDistribution;

    metrics::Operation insertOperation;
};

void MultiCollectionInsert::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            // Select a collection
            auto collectionNumber = config->uniformDistribution(_rng);
            auto collectionName = "Collection" + std::to_string(collectionNumber);
            // BOOST_LOG_TRIVIAL(info) << "Collection Name is " << collectionName;
            auto collection = config->database[collectionName];

            auto docs = std::vector<bsoncxx::document::view_or_value>{};
            docs.reserve(config->batchSize);

            size_t bytes = 0;
            for (uint j = 0; j < config->batchSize; j++) {
                auto newDoc = config->docExpr();
                bytes += newDoc.view().length();
                docs.push_back(std::move(newDoc));
            }
            {
                auto opCtx = config->insertOperation.start();
                auto result = collection.insert_many(std::move(docs), config->insertOptions);
                if (result) {
                    opCtx.addDocuments(result->inserted_count());
                }
                opCtx.addBytes(bytes);
                opCtx.success();
            }
        }
    }
}

MultiCollectionInsert::MultiCollectionInsert(genny::ActorContext& context)
    : Actor(context),
      _client{std::move(context.client())},
      _loop{context, _client, MultiCollectionInsert::id()} {}

namespace {
auto registerMultiCollectionInsert =
    genny::Cast::registerDefault<genny::actor::MultiCollectionInsert>();
}
}  // namespace genny::actor
