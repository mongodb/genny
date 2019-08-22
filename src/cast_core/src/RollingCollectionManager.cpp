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
    metrics::Operation deleteCollectionOperation;
    metrics::Operation createCollectionOperation;
    bool setupPhase;
    DocumentGenerator documentExpr;
    int64_t documentCount;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db}, setupPhase{phaseContext["Setup"].maybe<bool>().value_or(false)},
          deleteCollectionOperation{phaseContext.operation("CreateCollection", id)},
          createCollectionOperation{phaseContext.operation("DeleteCollection", id)},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)},
          documentCount{phaseContext["DocumentCount"].maybe<IntegerSpec>().value_or(0)} {}
};

std::string getRollingCollectionName(int64_t lastId){
    return "r_" + std::to_string(lastId);
}

mongocxx::collection createCollection(mongocxx::database& database, std::vector<DocumentGenerator>& indexConfig,
                                      std::string collectionName){
    auto collection = database.create_collection(collectionName);
    for (auto&& keys : indexConfig) {
        collection.create_index(keys());
    }
}

void RollingCollectionManager::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->setupPhase) {
                BOOST_LOG_TRIVIAL(info) << "Creating " << _collectionWindowSize << " initial collections.";
                for (auto i = 0; i < _collectionWindowSize; ++i){
                    auto collectionName = getRollingCollectionName(i);
                    auto collection = createCollection(config->database, _indexConfig, collectionName);
                    _collectionNames.push_back(collectionName);
                    for (auto j = 0; j < config->documentCount; ++j){
                        auto document = config->documentExpr();
                        collection.insert_one(document.view());
                    }
                    _currentCollectionId++;
                }
            } else {
                // Create collection with timestamped name
                auto createCollectionTracker = config->createCollectionOperation.start();
                auto collectionName = getRollingCollectionName(_currentCollectionId);
                createCollection(config->database, _indexConfig, collectionName);
                _currentCollectionId++;
                createCollectionTracker.success();
                _collectionNames.push_back(collectionName);
                // Delete collection at head of deque
                collectionName = _collectionNames.front();
                _collectionNames.pop_front();
                auto deleteCollectionTracker = config->deleteCollectionOperation.start();
                config->database[collectionName].drop();
                deleteCollectionTracker.success();
            }
        }
    }
}

RollingCollectionManager::RollingCollectionManager(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _currentCollectionId{0},
      _collectionWindowSize{context["CollectionWindowSize"].maybe<IntegerSpec>().value_or(0)},
      _collectionNames{WorkloadContext::getActorSharedState<RollingCollectionManager, RollingCollectionNames>()},
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
