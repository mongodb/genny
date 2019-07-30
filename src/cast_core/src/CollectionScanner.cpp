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

struct CollectionScanner::PhaseConfig {
    std::vector<mongocxx::collection> collections;
    bool skipFirstLoop = false;
    metrics::Operation scanOperation;
    int actorIndex;

    PhaseConfig(PhaseContext& context,
                const mongocxx::database& db,
                ActorId id,
                int collectionCount,
                int threads,
                ActorCounter& counter)
        : skipFirstLoop{context["SkipFirstLoop"].maybe<bool>().value_or(false)},
          scanOperation{context.operation("Scan", id)} {
        // This tracks which CollectionScanners we are out of all CollectionScanners. As opposed to
        // ActorId which is the overall actorId in the entire genny workload.
        int actorIndex = counter;
        counter++;
        // Distribute the collections among the actors.
        for (const auto& collectionName :
             distributeCollectionNames(collectionCount, threads, actorIndex)) {
            collections.push_back(db[collectionName]);
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
            BOOST_LOG_TRIVIAL(info) << "Starting collection scanner id: " << config->actorIndex;
            // Count over all collections this thread has been tasked with scanning each.
            auto statTracker = config->scanOperation.start();
            for (auto& collection : config->collections) {
                statTracker.addDocuments(collection.count_documents({}));
            }
            statTracker.success();
            _runningActorCounter--;
            BOOST_LOG_TRIVIAL(info) << "Finished collection scanner id: " << config->actorIndex;
        }
    }
}

CollectionScanner::CollectionScanner(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", CollectionScanner::id())},
      _client{context.client()},
      _actorCounter{WorkloadContext::getActorSharedState<CollectionScanner, ActorCounter>()},
      _runningActorCounter{
          WorkloadContext::getActorSharedState<CollectionScanner, RunningActorCounter>()},
      _loop{context,
            (*_client)[context["Database"].to<std::string>()],
            CollectionScanner::id(),
            context["CollectionCount"].to<IntegerSpec>(),
            context["Threads"].to<IntegerSpec>(),
            _actorCounter} {
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
