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

#include <cast_core/actors/MonotonicSingleLoader.hpp>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/builder/stream/document.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {

struct MonotonicSingleLoader::PhaseConfig {
    PhaseConfig(PhaseContext& phaseContext, mongocxx::pool::entry& client, ActorId id);

    mongocxx::database db;
    mongocxx::collection collection;
    int64_t batchSize;
    int64_t numDocuments;
    DocumentGenerator documentExpr;
};

MonotonicSingleLoader::PhaseConfig::PhaseConfig(PhaseContext& phaseContext,
                                                mongocxx::pool::entry& client,
                                                ActorId id)
    : db{client->database(phaseContext["Database"].to<std::string>())},
      // The default collection name of "Collection0" is for consistency with Loader and
      // MonotonicLoader.
      collection{
          db.collection(phaseContext["Collection"].maybe<std::string>().value_or("Collection0"))},
      batchSize{phaseContext["BatchSize"].to<IntegerSpec>()},
      numDocuments{phaseContext["DocumentCount"].to<IntegerSpec>()},
      documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}

void MonotonicSingleLoader::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto totalOpCtx = _totalBulkLoad.start();

            int64_t lowId;
            while ((lowId = _docIdCounter.fetch_add(config->batchSize)) < config->numDocuments) {
                auto highId = std::min(lowId + config->batchSize, config->numDocuments) - 1;

                std::vector<bsoncxx::document::value> docs;
                docs.reserve(config->batchSize);

                size_t numBytes = 0;
                for (auto id = lowId; id <= highId; ++id) {
                    auto builder = bsoncxx::builder::stream::document();
                    builder << "_id" << id;
                    builder << bsoncxx::builder::concatenate(config->documentExpr());
                    bsoncxx::document::value doc = builder << bsoncxx::builder::stream::finalize;

                    numBytes += doc.view().length();
                    docs.push_back(std::move(doc));
                }

                {
                    auto individualOpCtx = _individualBulkLoad.start();
                    auto result = config->collection.insert_many(std::move(docs));

                    totalOpCtx.addBytes(numBytes);
                    individualOpCtx.addBytes(numBytes);
                    if (result) {
                        totalOpCtx.addDocuments(result->inserted_count());
                        individualOpCtx.addDocuments(result->inserted_count());
                    }

                    individualOpCtx.success();
                }
            }

            totalOpCtx.success();
        }
    }
}

MonotonicSingleLoader::MonotonicSingleLoader(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _totalBulkLoad{context.operation("TotalBulkInsert", MonotonicSingleLoader::id())},
      _individualBulkLoad{context.operation("IndividualBulkInsert", MonotonicSingleLoader::id())},
      _docIdCounter{
          WorkloadContext::getActorSharedState<MonotonicSingleLoader,
                                               MonotonicSingleLoader::DocumentIdCounter>()},
      _loop{context, _client, MonotonicSingleLoader::id()} {}

namespace {
auto registerMonotonicSingleLoader = Cast::registerDefault<MonotonicSingleLoader>();
}  // namespace
}  // namespace genny::actor
