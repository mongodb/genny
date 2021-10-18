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

#include <cast_core/actors/MonotonicLoader.hpp>

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

#include <bsoncxx/builder/stream/document.hpp>

namespace genny::actor {

/** @private */
using index_type = std::pair<DocumentGenerator, std::optional<DocumentGenerator>>;
using namespace bsoncxx;

/** @private */
struct MonotonicLoader::PhaseConfig {
    PhaseConfig(PhaseContext& context, mongocxx::pool::entry& client, uint thread, ActorId id)
        : database{(*client)[context["Database"].to<std::string>()]},
          // The next line uses integer division. The Remainder is accounted for below.
          numCollections{context["CollectionCount"].to<IntegerSpec>() /
                         context["Threads"].to<IntegerSpec>()},
          numDocuments{context["DocumentCount"].to<IntegerSpec>()},
          batchSize{context["BatchSize"].to<IntegerSpec>()},
          documentExpr{context["Document"].to<DocumentGenerator>(context, id)},
          collectionOffset{numCollections * thread} {
        auto& indexNodes = context["Indexes"];
        for (auto [k, indexNode] : indexNodes) {
            indexes.emplace_back(indexNode["keys"].to<DocumentGenerator>(context, id),
                                 indexNode["options"].maybe<DocumentGenerator>(context, id));
        }
        if (thread == context["Threads"].to<int>() - 1) {
            // Pick up any extra collections left over by the division
            numCollections += context["CollectionCount"].to<uint>() % context["Threads"].to<uint>();
        }
    }

    mongocxx::database database;
    int64_t numCollections;
    int64_t numDocuments;
    int64_t batchSize;
    DocumentGenerator documentExpr;
    std::vector<index_type> indexes;
    int64_t collectionOffset;
};

void genny::actor::MonotonicLoader::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            for (uint i = config->collectionOffset;
                 i < config->collectionOffset + config->numCollections;
                 i++) {
                auto collectionName = "Collection" + std::to_string(i);
                auto collection = config->database[collectionName];
                // Insert the documents
                uint remainingInserts = config->numDocuments;
                int id_num = 0;
                {
                    auto totalOpCtx = _totalBulkLoad.start();
                    while (remainingInserts > 0) {
                        // insert the next batch
                        int64_t numberToInsert =
                            std::min<int64_t>(config->batchSize, remainingInserts);
                        auto docs = std::vector<document::view_or_value>{};
                        docs.reserve(remainingInserts);
                        for (uint j = 0; j < numberToInsert; j++) {
                            auto tmpDoc = config->documentExpr();
                            auto builder = builder::stream::document();
                            builder << "_id" << ++id_num;
                            builder << builder::concatenate(tmpDoc.view());
                            document::value newDoc = builder
                                << builder::stream::finalize;
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
                // Make the index
                bool _indexReq = false;
                builder::stream::document builder{};
                auto indexCmd = builder << "createIndexes" << collectionName
                                        <<  "indexes" << builder::stream::open_array;
                for (auto&& [keys, options] : config->indexes) {
                    _indexReq = true;
                    auto indexKey = keys();
                    if (options) {
                        auto indexOptions = (*options)();
                        indexCmd = indexCmd << builder::stream::open_document
                                            << "key" << indexKey.view()
                                            << builder::concatenate(indexOptions.view())
                                            << builder::stream::close_document;
                    } else {
                        std::string index_name = "";
                        for (auto field : indexKey.view()) {
                            index_name = index_name + field.key().to_string();
                        }
                        indexCmd = indexCmd << builder::stream::open_document
                                            << "key" << indexKey.view()
                                            << "name" << index_name
                                            << builder::stream::close_document;
                    }
                }
                auto doc = indexCmd << builder::stream::close_array << builder::stream::finalize;
                if (_indexReq) {
                    BOOST_LOG_TRIVIAL(debug)
                            << "Building index" << to_json(doc.view());
                    auto indexOpCtx = _indexBuild.start();
                    config->database.run_command(doc.view());
                    indexOpCtx.success();
                }
                BOOST_LOG_TRIVIAL(info) << "Done with load phase. All documents loaded";
            }
        }
    }
}

MonotonicLoader::MonotonicLoader(genny::ActorContext& context, uint thread)
    : Actor(context),
      _totalBulkLoad{context.operation("TotalBulkInsert", MonotonicLoader::id())},
      _individualBulkLoad{context.operation("IndividualBulkInsert", MonotonicLoader::id())},
      _indexBuild{context.operation("IndexBuild", MonotonicLoader::id())},
      _client{std::move(context.client())},
      _loop{context, _client, thread, MonotonicLoader::id()} {}

class MonotonicLoaderProducer : public genny::ActorProducer {
public:
    MonotonicLoaderProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context["Type"].to<std::string>() != "MonotonicLoader") {
            return {};
        }
        genny::ActorVector out;
        for (uint i = 0; i < context["Threads"].to<int>(); ++i) {
            out.emplace_back(std::make_unique<genny::actor::MonotonicLoader>(context, i));
        }
        return out;
    }
};

namespace {
std::shared_ptr<genny::ActorProducer> monotonicLoaderProducer =
    std::make_shared<MonotonicLoaderProducer>("MonotonicLoader");
auto registration = genny::Cast::registerCustom<genny::ActorProducer>(monotonicLoaderProducer);
}  // namespace
}  // namespace genny::actor
