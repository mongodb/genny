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

#include <cast_core/actors/CollectionScanner.hpp>

#include <chrono>
#include <memory>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {
enum ScanType { Count, Snapshot, Standard };

struct CollectionScanner::PhaseConfig {
    std::vector<mongocxx::collection> collections;
    std::optional<DocumentGenerator> filterExpr;
    mongocxx::database database;
    bool skipFirstLoop = false;
    metrics::Operation scanOperation;
    metrics::Operation exceptionsCaught;
    int64_t documents;
    int64_t scanSizeBytes;
    ScanType scanType;
    bool queryCollectionList;
    std::optional<mongocxx::options::transaction> transactionOptions;

    PhaseConfig(PhaseContext& context,
                const CollectionScanner* actor,
                const mongocxx::database& db,
                int collectionCount,
                int threads,
                bool generateCollectionNames)
        : database{db},
          skipFirstLoop{context["SkipFirstLoop"].maybe<bool>().value_or(false)},
          filterExpr{context["Filter"].maybe<DocumentGenerator>(context, actor->id())},
          scanOperation{context.operation("Scan", actor->id())},
          exceptionsCaught{context.operation("ExceptionsCaught", actor->id())},
          documents{context["Documents"].maybe<IntegerSpec>().value_or(0)},
          scanSizeBytes{context["ScanSizeBytes"].maybe<IntegerSpec>().value_or(0)} {
        // Initialise scan type enum.
        auto scanTypeString = context["ScanType"].to<std::string>();
        // Ignore case
        boost::algorithm::to_lower(scanTypeString);
        if (scanTypeString == "count") {
            scanType = Count;
        } else if (scanTypeString == "snapshot") {
            transactionOptions = mongocxx::options::transaction{};
            auto readConcern = mongocxx::read_concern{};
            readConcern.acknowledge_level(mongocxx::read_concern::level::k_majority);
            transactionOptions->read_concern(readConcern);
            scanType = Snapshot;
        } else if (scanTypeString == "standard") {
            scanType = Standard;
        } else {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("ScanType is invalid."));
        }
        if (generateCollectionNames) {
            queryCollectionList = false;
            BOOST_LOG_TRIVIAL(info) << " Generating collection names";
            for (const auto& collectionName :
                 distributeCollectionNames(collectionCount, threads, actor->_index)) {
                collections.push_back(db[collectionName]);
            }
        } else {
            queryCollectionList = true;
        }
    }
};

void collectionScan(genny::v1::ActorPhase<CollectionScanner::PhaseConfig>& config,
                    std::vector<mongocxx::collection>& collections) {
    /*
     * Here we are either doing a snapshot collection scan
     * or just a normal scan?
     */
    size_t docCount = 0;
    size_t scanSize = 0;
    bool scanFinished = false;
    auto statTracker = config->scanOperation.start();
    auto exceptionsCaught = config->exceptionsCaught.start();
    for (auto& collection : config->collections) {
        auto filter = config->filterExpr ? config->filterExpr->evaluate()
                                         : bsoncxx::document::view_or_value{};
        auto docs = collection.find(filter);
        /*
         * Try-catch this as the collection may have been deleted.
         * You can still do a find but it'll throw an exception when we iterate.
         */
        try {
            for (auto& doc : docs) {
                docCount += 1;
                scanSize += doc.length();
                if (config->documents != 0 && config->documents == docCount) {
                    scanFinished = true;
                    break;
                }
                if (config->scanSizeBytes != 0 && scanSize >= config->scanSizeBytes) {
                    scanFinished = true;
                    break;
                }
            }
            if (scanFinished) {
                break;
            }
        } catch (const mongocxx::operation_exception& e) {
            exceptionsCaught.addDocuments(1);
            exceptionsCaught.success();
        }
    }
    statTracker.addBytes(scanSize);
    statTracker.addDocuments(docCount);
    statTracker.success();
}

void countScan(genny::v1::ActorPhase<CollectionScanner::PhaseConfig>& config,
               std::vector<mongocxx::collection> collections) {
    auto statTracker = config->scanOperation.start();
    auto exceptionsCaught = config->exceptionsCaught.start();
    for (auto& collection : config->collections) {
        auto filter = config->filterExpr ? config->filterExpr->evaluate()
                                         : bsoncxx::document::view_or_value{};
        try {
            statTracker.addDocuments(collection.count_documents(filter));
        } catch (const mongocxx::operation_exception& e) {
            exceptionsCaught.addDocuments(1);
            exceptionsCaught.success();
        }
    }
    statTracker.success();
}

void CollectionScanner::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->skipFirstLoop) {
                config->skipFirstLoop = false;
                continue;
            }
            _runningActorCounter++;
            BOOST_LOG_TRIVIAL(info) << "Starting collection scanner id: " << this->_index;
            std::this_thread::sleep_for(std::chrono::seconds{1});

            // Populate collections if need be.
            std::vector<mongocxx::collection>& collections = config->collections;
            if (config->queryCollectionList) {
                std::vector<mongocxx::collection> tempCollections{};
                for (const auto& collection : config->database.list_collection_names({})) {
                    tempCollections.push_back(config->database[collection]);
                }
                collections = tempCollections;
            }

            // Do each kind of scan.
            if (config->scanType == Count) {
                countScan(config, collections);
            } else if (config->scanType == Snapshot) {
                mongocxx::client_session session = _client->start_session({});
                session.start_transaction(*config->transactionOptions);
                collectionScan(config, collections);
                session.commit_transaction();
            } else {
                collectionScan(config, collections);
            }
            _runningActorCounter--;
            BOOST_LOG_TRIVIAL(info) << "Finished collection scanner id: " << this->_index;
        }
    }
}

CollectionScanner::CollectionScanner(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", CollectionScanner::id())},
      _client{context.client()},
      _index{WorkloadContext::getActorSharedState<CollectionScanner, ActorCounter>().fetch_add(1)},
      _runningActorCounter{
          WorkloadContext::getActorSharedState<CollectionScanner, RunningActorCounter>()},
      _generateCollectionNames{context["GenerateCollectionNames"].maybe<bool>().value_or(false)},
      _loop{context,
            this,
            (*_client)[context["Database"].to<std::string>()],
            context["CollectionCount"].maybe<IntegerSpec>().value_or(0),
            context["Threads"].to<IntegerSpec>(),
            context["GenerateCollectionNames"].maybe<bool>().value_or(false)} {
    _runningActorCounter.store(0);
}

std::vector<std::string> distributeCollectionNames(size_t collectionCount,
                                                   size_t threadCount,
                                                   ActorId actorId) {
    // We always want a fair division of collections to actors
    std::vector<std::string> collectionNames{};
    if ((threadCount > collectionCount && threadCount % collectionCount != 0) ||
        collectionCount % threadCount != 0) {
        throw std::invalid_argument("Thread count must be mutliple of database collection count");
    }
    int collectionsPerActor = threadCount > collectionCount ? 1 : collectionCount / threadCount;
    int collectionIndexStart = (actorId % collectionCount) * collectionsPerActor;
    int collectionIndexEnd = collectionIndexStart + collectionsPerActor;
    for (int i = collectionIndexStart; i < collectionIndexEnd; ++i) {
        collectionNames.push_back("Collection" + std::to_string(i));
    }
    return collectionNames;
}

namespace {
auto registerCollectionScanner = Cast::registerDefault<CollectionScanner>();
}  // namespace
}  // namespace genny::actor
