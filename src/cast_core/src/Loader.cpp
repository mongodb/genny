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

#include <cast_core/actors/Loader.hpp>

#include <algorithm>
#include <memory>

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
using index_type = std::pair<DocumentGenerator, std::optional<DocumentGenerator>>;

/** @private */
struct Loader::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                mongocxx::pool::entry& client,
                uint thread,
                DefaultRandom& rng)
        : _rng{rng},
          database{(*client)[context.get<std::string>("Database")]},
          // The next line uses integer division. The Remainder is accounted for below.
          numCollections{context.get<IntegerSpec, true>("CollectionCount") /
                         context.get<IntegerSpec, true>("Threads")},
          numDocuments{context.get<IntegerSpec, true>("DocumentCount")},
          batchSize{context.get<IntegerSpec, true>("BatchSize")},
          documentExpr{Generators::document(context.get("Document"), _rng)},
          collectionOffset{numCollections * thread} {
        auto indexNodes = context.get("Indexes");
        for (auto indexNode : indexNodes) {
            indexes.emplace_back(Generators::document(indexNode["keys"], _rng),
                                 indexNode["options"] ? std::make_optional(Generators::document(
                                                            indexNode["options"], _rng))
                                                      : std::nullopt);
        }
        if (thread == context.get<int>("Threads") - 1) {
            // Pick up any extra collections left over by the division
            numCollections += context.get<uint>("CollectionCount") % context.get<uint>("Threads");
        }
    }

    DefaultRandom& _rng;
    mongocxx::database database;
    int64_t numCollections;
    int64_t numDocuments;
    int64_t batchSize;
    DocumentGenerator documentExpr;
    std::vector<index_type> indexes;
    int64_t collectionOffset;
};

void genny::actor::Loader::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            for (uint i = config->collectionOffset;
                 i < config->collectionOffset + config->numCollections;
                 i++) {
                auto collectionName = "Collection" + std::to_string(i);
                auto collection = config->database[collectionName];
                // Insert the documents
                uint remainingInserts = config->numDocuments;
                {
                    auto totalOpCtx = _totalBulkLoad.start();
                    while (remainingInserts > 0) {
                        // insert the next batch
                        int64_t numberToInsert =
                            std::min<int64_t>(config->batchSize, remainingInserts);
                        auto docs = std::vector<bsoncxx::document::view_or_value>{};
                        docs.reserve(remainingInserts);
                        for (uint j = 0; j < numberToInsert; j++) {
                            auto newDoc = config->documentExpr();
                            docs.push_back(std::move(newDoc));
                        }
                        {
                            auto individualOpCtx = _individualBulkLoad.start();
                            auto result = collection.insert_many(std::move(docs));
                            remainingInserts -= result->inserted_count();
                            individualOpCtx.success();
                        }
                    }
                    totalOpCtx.success();
                }
                // For each index
                for (auto&& [keys, options] : config->indexes) {
                    // Make the index
                    auto indexKey = keys();
                    BOOST_LOG_TRIVIAL(debug)
                        << "Building index " << bsoncxx::to_json(indexKey.view());
                    if (options) {
                        auto indexOptions = (*options)();
                        BOOST_LOG_TRIVIAL(debug)
                            << "With options " << bsoncxx::to_json(indexOptions.view());
                        auto indexOpCtx = _indexBuild.start();
                        collection.create_index(std::move(indexKey), std::move(indexOptions));
                        indexOpCtx.success();
                    } else {
                        auto indexOpCtx = _indexBuild.start();
                        collection.create_index(std::move(indexKey));
                        indexOpCtx.success();
                    }
                }
            }
            BOOST_LOG_TRIVIAL(info) << "Done with load phase. All documents loaded";
        }
    }
}

Loader::Loader(genny::ActorContext& context, uint thread)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _totalBulkLoad{context.operation("TotalBulkInsert", Loader::id())},
      _individualBulkLoad{context.operation("IndividualBulkInsert", Loader::id())},
      _indexBuild{context.operation("IndexBuild", Loader::id())},
      _client{std::move(context.client())},
      _loop{context, _client, thread, _rng} {}

class LoaderProducer : public genny::ActorProducer {
public:
    LoaderProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context.get<std::string>("Type") != "Loader") {
            return {};
        }
        genny::ActorVector out;
        for (uint i = 0; i < context.get<int>("Threads"); ++i) {
            out.emplace_back(std::make_unique<genny::actor::Loader>(context, i));
        }
        return out;
    }
};

namespace {
std::shared_ptr<genny::ActorProducer> loaderProducer = std::make_shared<LoaderProducer>("Loader");
auto registration = genny::Cast::registerCustom<genny::ActorProducer>(loaderProducer);
}  // namespace
}  // namespace genny::actor
