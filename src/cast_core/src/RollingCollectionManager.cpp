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

#include <cast_core/actors/RollingCollectionManager.hpp>

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

#include <chrono>
#include <ctime>
#include <iostream>

namespace genny::actor {
struct RollingCollectionManager::PhaseConfig {
    mongocxx::database database;
    std::deque<mongocxx::collection> collections;
    metrics::Operation deleteCollectionOperation;
    metrics::Operation createCollectionOperation;
    bool setupPhase;
    int64_t collectionCount;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db}, setupPhase{phaseContext["Setup"].maybe<bool>().value_or(false)},
        deleteCollectionOperation{phaseContext.operation("CreateCollection", id)},
        createCollectionOperation{phaseContext.operation("DeleteCollection", id)},
        collectionCount{phaseContext["CollectionCount"].maybe<IntegerSpec>().value_or(0)} {}
};

std::string getCollectionName(){
    std::string timestamp(20 , '.');
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time = std::chrono::system_clock::to_time_t(now);
    std::strftime(&timestamp[0], timestamp.size(), "%Y-%m-%d-%H:%M:%S", std::localtime(&time));
    timestamp[timestamp.size() - 1] = '.';
    std::stringstream ss;
    ss << "prefix-" << timestamp << std::setfill('0') << std::setw(3)  << ms.count();
    return ss.str();
}

mongocxx::collection createCollection(mongocxx::database& database, std::vector<DocumentGenerator>& indexConfig){
    auto collection = database.create_collection(getCollectionName());
    for (auto&& keys : indexConfig) {
        collection.create_index(keys());
    }
    return collection;
}

void RollingCollectionManager::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->setupPhase) {
                BOOST_LOG_TRIVIAL(info) << "Creating " << config->collectionCount << " initial collections";
                for (int i = 0; i < config->collectionCount; ++i){
                    try {
                        _collections.push_back(createCollection(config->database, _indexConfig));
                    } catch (const std::exception& ex) {
                        BOOST_LOG_TRIVIAL(info) << "Created duplicate collection.";
                        i--;
                    }
                }
            } else {
                // Create collection with timestamped name
                auto createCollectionTracker = config->createCollectionOperation.start();
                auto collection = createCollection(config->database, _indexConfig);
                createCollectionTracker.success();
                _collections.push_back(collection);

                // Delete collection at head of deque
                collection = _collections.front();
                _collections.pop_front();
                auto deleteCollectionTracker = config->deleteCollectionOperation.start();
                collection.drop();
                deleteCollectionTracker.success();
            }
        }
    }
}

RollingCollectionManager::RollingCollectionManager(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], RollingCollectionManager::id()} {
        _indexConfig = std::vector<DocumentGenerator>{};
        auto& indexNodes = context["Indexes"];
        for (auto [k, indexNode] : indexNodes) {
            _indexConfig.emplace_back(indexNode["keys"].to<DocumentGenerator>(context, RollingCollectionManager::id()));
        }
      }

namespace {
auto registerRollingCollectionManager = Cast::registerDefault<RollingCollectionManager>();
}  // namespace
}  // namespace genny::actor
