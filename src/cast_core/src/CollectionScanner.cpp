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


namespace genny::actor {
enum ScanType { Count, Snapshot, Standard };


struct CollectionScanner::PhaseConfig {
    std::vector<mongocxx::collection> collections;
    bool skipFirstLoop = false;
    metrics::Operation scanOperation;
    size_t documents;
    size_t scanSizeBytes;
    ScanType scanType;

    PhaseConfig(PhaseContext& context,
                const CollectionScanner* actor,
                const mongocxx::database& db,
                int collectionCount,
                int threads,
                bool generateCollectionNames)
        : skipFirstLoop{context["SkipFirstLoop"].maybe<bool>().value_or(false)},
          scanOperation{context.operation("Scan", actor->id())},
          documents{context["Documents"].maybe<IntegerSpec>().value_or(0)},
          scanSizeBytes{context["ScanSizeBytes"].maybe<IntegerSpec>().value_or(0)} {
        // Initialise scan type enum.
        auto scanTypeString = context["ScanType"].to<std::string>();
        if (scanTypeString == "Count"){
            scanType = Count;
        } else if (scanTypeString == "Snapshot"){
            scanType = Snapshot;
        } else {
            scanType = Standard;
        }
        // This tracks which CollectionScanners we are out of all CollectionScanners. As opposed to
        // ActorId which is the overall actorId in the entire genny workload.
        // Distribute the collections among the actors.
        if (generateCollectionNames){
            BOOST_LOG_TRIVIAL(info) << " Generating collection names";
            for (const auto& collectionName :
                distributeCollectionNames(collectionCount, threads, actor->_index)) {
                collections.push_back(db[collectionName]);
            }
        }
    }
};

void CollectionScanner::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->skipFirstLoop) {
                config->skipFirstLoop = false;
                continue;
            }
            _runningActorCounter++;
            BOOST_LOG_TRIVIAL(info) << "Starting collection scanner id: " << this->_index;
            // Count over all collections this thread has been tasked with scanning each.
            std::this_thread::sleep_for(std::chrono::seconds{1});
            if (config->scanType == Count){
                BOOST_LOG_TRIVIAL(info) << "Scan type is standard";
                auto statTracker = config->scanOperation.start();
                for (auto& collection : config->collections) {
                    statTracker.addDocuments(collection.count_documents({}));
                }
                statTracker.success();
            } else {
                if (config->scanType == Snapshot) {
                    auto session = _client.start_session();
                }
                BOOST_LOG_TRIVIAL(info) << "Scan type is standard";
                //Here we are either doing a snapshot collection scan
                // or just a normal scan?
                size_t docCount = 0;
                size_t scanSize = 0;
                bool scanFinished = false;
                auto statTracker = config->scanOperation.start();
                for (auto& collection : config->collections) {
                    BOOST_LOG_TRIVIAL(info) << "Iterating over collecitons";
                    auto docs = collection.find({});
                    for (auto &doc : docs){
                        docCount += 1;
                        if (config->documents != 0 && config->documents == docCount) {
                            scanFinished = true;
                            break;
                        }
                        if (config->scanSizeBytes != 0) {
                            scanSize += doc.length();
                            if (scanSize >= config->scanSizeByte) {
                                scanFinished = true;
                                break;
                            }
                        }
                    }
                    if (scanFinished){
                        break;
                    }
                }
                statTracker.addDocuments(docCount);
                statTracker.success();
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
            context["CollectionCount"].to<IntegerSpec>(),
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
