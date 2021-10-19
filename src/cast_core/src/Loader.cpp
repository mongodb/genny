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

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/** @private */
using index_type = std::pair<DocumentGenerator, std::optional<DocumentGenerator>>;

/** @private */
struct Loader::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                mongocxx::pool::entry& client,
                uint thread,
                size_t totalThreads,
                ActorId id)
        : database{(*client)[context["Database"].to<std::string>()]},
          multipleThreadsPerCollection{
              context["MultipleThreadsPerCollection"].maybe<bool>().value_or(false)},
          // The next line uses integer division for non multithreaded configurations.
          // The Remainder is accounted for below.
          numCollections{multipleThreadsPerCollection
                             ? 1
                             : context["CollectionCount"].to<IntegerSpec>() /
                                 context["Threads"].to<IntegerSpec>()},
          numDocuments{context["DocumentCount"].to<IntegerSpec>()},
          batchSize{context["BatchSize"].to<IntegerSpec>()},
          documentExpr{context["Document"].to<DocumentGenerator>(context, id)},
          collectionOffset{multipleThreadsPerCollection
                               ? thread % context["CollectionCount"].to<IntegerSpec>()
                               : numCollections * thread} {
        auto createIndexes = [&]() {
            auto& indexNodes = context["Indexes"];
            for (auto [k, indexNode] : indexNodes) {
                indexes.emplace_back(indexNode["keys"].to<DocumentGenerator>(context, id),
                                     indexNode["options"].maybe<DocumentGenerator>(context, id));
            }
        };

        if (multipleThreadsPerCollection) {
            if (context["Threads"]) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "Phase Config 'Threads' parameter is not supported if "
                    "'MultipleThreadsPerCollection' is true."));
            }
            uint collectionCount = context["CollectionCount"].to<IntegerSpec>();
            if (totalThreads % collectionCount != 0) {
                std::ostringstream ss;
                ss << "'CollectionCount' (" << collectionCount
                   << ") must be an even divisor of 'Threads' (" << totalThreads << ").";
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(ss.str()));
            }
            uint numThreads = totalThreads / collectionCount;
            numDocuments = context["DocumentCount"].to<int64_t>() / numThreads;
            if (thread / collectionCount == 0) {
                // The first thread for each collection:
                //   1. Picks up any extra documents left over by the division.
                //   2. Is responsible for creating the indexes.
                numDocuments += context["DocumentCount"].to<uint>() % numThreads;
                createIndexes();
            }
        } else {
            createIndexes();
            if (thread == context["Threads"].to<int>() - 1) {
                // Pick up any extra collections left over by the division
                numCollections +=
                    context["CollectionCount"].to<uint>() % context["Threads"].to<uint>();
            }
        }
    }

    mongocxx::database database;
    bool multipleThreadsPerCollection;
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
                BOOST_LOG_TRIVIAL(info)
                    << "Starting to insert: " << config->numDocuments << " docs "
                    << "into " << collectionName;
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
                BOOST_LOG_TRIVIAL(info) << "Done with load phase. All " << config->numDocuments
                                        << " documents loaded into " << collectionName;
            }
        }
    }
}

Loader::Loader(genny::ActorContext& context, uint thread, size_t totalThreads)
    : Actor(context),
      _totalBulkLoad{context.operation("TotalBulkInsert", Loader::id())},
      _individualBulkLoad{context.operation("IndividualBulkInsert", Loader::id())},
      _indexBuild{context.operation("IndexBuild", Loader::id())},
      _client{std::move(context.client())},
      _loop{context, _client, thread, totalThreads, Loader::id()} {}

class LoaderProducer : public genny::ActorProducer {
public:
    LoaderProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context["Type"].to<std::string>() != "Loader") {
            return {};
        }
        genny::ActorVector out;
        uint totalThreads = context["Threads"].to<int>();
        for (uint i = 0; i < totalThreads; ++i) {
            out.emplace_back(std::make_unique<genny::actor::Loader>(context, i, totalThreads));
        }
        return out;
    }
};

namespace {
std::shared_ptr<genny::ActorProducer> loaderProducer = std::make_shared<LoaderProducer>("Loader");
auto registration = genny::Cast::registerCustom<genny::ActorProducer>(loaderProducer);
}  // namespace
}  // namespace genny::actor
