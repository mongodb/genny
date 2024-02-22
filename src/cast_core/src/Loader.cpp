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
#include <bsoncxx/builder/stream/document.hpp>
#include <chrono>

namespace genny::actor {

/** @private */
using index_type = std::tuple<DocumentGenerator, std::optional<DocumentGenerator>, std::string>;
using namespace bsoncxx;

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
                std::string indexName = "";
                for (auto [key, value] : indexNode["keys"]) {
                    auto key_value = key.toString() + "_" + value.to<std::string>();
                    indexName = indexName.empty() ? key_value : indexName + "_" + key_value;
                }
                indexes.emplace_back(
                    indexNode["keys"].to<DocumentGenerator>(context, id),
                    indexNode["options"].maybe<DocumentGenerator>(context, id),
                    indexNode["options"]["name"].maybe<std::string>().value_or(indexName)
                );
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
            numDocuments = context["DocumentCount"].to<IntegerSpec>() / numThreads;
            if (thread / collectionCount == 0) {
                // The first thread for each collection:
                //   1. Picks up any extra documents left over by the division.
                //   2. Is responsible for creating the indexes.
                numDocuments += context["DocumentCount"].to<IntegerSpec>() % numThreads;
                createIndexes();
            }
        } else {
            if (context["Threads"] && context["Threads"].to<IntegerSpec>() > totalThreads) {
                BOOST_THROW_EXCEPTION(
                                      InvalidConfigurationException("Phase Config 'Threads' parameter must be less than "
                                                                    "or equal to Actor Config 'Threads' in Loader actor"));
            }
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
        if (!config.isNop()) {
            // Warm up the connection to the database
            auto ping = builder::stream::document{} << "ping" << 1 << builder::stream::finalize;
            config->database.run_command(ping.view());
        }
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
                auto start = std::chrono::high_resolution_clock::now();
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
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<float> duration = end - start;
                float seconds = duration.count();
                BOOST_LOG_TRIVIAL(info) << "Done with load phase. All " << config->numDocuments
                                        << " documents loaded into " << collectionName
                                        << " in " << seconds << " seconds"
                                        << std::setprecision(5)
                                        << " at " << ((float) config->numDocuments) / seconds << " docs/s";
                // Make the index
                bool _indexReq = false;
                builder::stream::document builder{};
                auto indexCmd = builder << "createIndexes" << collectionName
                                        <<  "indexes" << builder::stream::open_array;
                for (auto&& [keys, options, indexName] : config->indexes) {
                    _indexReq = true;
                    auto indexKey = keys();
                    if (options) {
                        auto indexOptions = (*options)();
                        indexCmd = indexCmd << builder::stream::open_document
                                            << "key" << indexKey.view()
                                            << "name" << indexName
                                            << builder::concatenate(indexOptions.view())
                                            << builder::stream::close_document;

                    } else {
                        indexCmd = indexCmd << builder::stream::open_document
                                            << "key" << indexKey.view()
                                            << "name" << indexName
                                            << builder::stream::close_document;
                    }
                }
                auto doc = indexCmd << builder::stream::close_array << builder::stream::finalize;
                if (_indexReq) {
                    BOOST_LOG_TRIVIAL(info)
                            << "Building index" << to_json(doc.view());
                    auto indexOpCtx = _indexBuild.start();
                    config->database.run_command(doc.view());
                    indexOpCtx.success();
                }
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
