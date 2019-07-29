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

#include <memory>
#include <chrono>

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
    mongocxx::database database;
    std::vector<std::string> collectionNames;
    bool skipFirstLoop = false;
    bool firstThread;
    bool lastThread = false;
    metrics::Operation scanOperation;
    int actorId;
    PhaseConfig(PhaseContext& context, const mongocxx::database& db, ActorId id, int collectionCount,
      int threads, ActorCounter &counter)
        : database{db},
          skipFirstLoop{context["SkipFirstLoop"].maybe<bool>().value_or(false)},
          scanOperation{context.operation("Scan", id)} {
        actorId = counter;
        counter ++;
        firstThread = actorId == 0;
        lastThread = actorId == threads - 1;
        // Distribute the collections among the actors.
        collectionNames = CollectionScanner::getCollectionNames(collectionCount, threads, actorId);
    }
};

void CollectionScanner::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->skipFirstLoop) {
                config->skipFirstLoop = false;
            } else {
                try {
                    _runningActorCounter ++;
                    BOOST_LOG_TRIVIAL(info) << "Starting collection scanner id:" << config->actorId;
                    // Count over all collections this thread has been tasked with scanning each.
                    auto statTracker = config->scanOperation.start();
                    for (int i = 0; i < config->collectionNames.size(); ++i){
                        statTracker.addDocuments(config->database[config->collectionNames[i]].count_documents({}));
                    }
                    _runningActorCounter --;
                    statTracker.success();
                } catch(mongocxx::operation_exception& e) {
                    BOOST_LOG_TRIVIAL(error) << boost::diagnostic_information(e);
                }
            }
        }
    }
}

CollectionScanner::CollectionScanner(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", CollectionScanner::id())},
      _client{context.client()},
      _actorCounter{WorkloadContext::getActorSharedState<CollectionScanner, ActorCounter>()},
      _runningActorCounter{WorkloadContext::getActorSharedState<CollectionScanner, RunningActorCounter>()},
      _loop{
          context,
          (*_client)[context["Database"].to<std::string>()],
          CollectionScanner::id(),
          context["CollectionCount"].to<IntegerSpec>(),
          context["Threads"].to<IntegerSpec>(),
          _actorCounter
      } {
          _runningActorCounter.store(0);
      }

namespace {
auto registerCollectionScanner = Cast::registerDefault<CollectionScanner>();
}  // namespace
}  // namespace genny::actor
