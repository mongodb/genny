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

#include <cast_core/actors/RollingCollectionReader.hpp>
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


namespace genny::actor {

struct RollingCollectionReader::PhaseConfig {
    std::optional<DocumentGenerator> filterExpr;
    mongocxx::database database;
    double distribution;
    metrics::Operation findOperation;
    boost::random::uniform_real_distribution<double> realDistribution;
    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db},
          filterExpr{phaseContext["Filter"].maybe<DocumentGenerator>(phaseContext, id)},
          distribution{phaseContext["Distribution"].maybe<double>().value_or(0)},
          findOperation{phaseContext.operation("Find", id)} {
        // Setup the real distribution.
        realDistribution =
            boost::random::uniform_real_distribution<double>(0, 1);
    }
};

int getNextCollectionId(size_t size, double distribution, double rand){
    return std::floor(size - ((distribution * rand) * size));
}

void RollingCollectionReader::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto id = getNextCollectionId(_rollingCollectionNames.size(),
              config->distribution, config->realDistribution(_random));
            auto collection = config->database[_rollingCollectionNames[id]];
            auto statTracker = config->findOperation.start();
            try {
                boost::optional<bsoncxx::document::value> optionalDocument;
                if (config->filterExpr) {
                    optionalDocument = collection.find_one(config->filterExpr->evaluate());
                } else {
                    optionalDocument = collection.find_one({});
                }
                if (optionalDocument) {
                    auto document = optionalDocument.get();
                    statTracker.addDocuments(1);
                    statTracker.addBytes(document.view().length());
                    statTracker.success();
                } else {
                    statTracker.failure();
                }
            } catch (mongocxx::operation_exception& e){
                //We likely tried to read from missing collection;
                statTracker.failure();
            }
        }
    }
}

RollingCollectionReader::RollingCollectionReader(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _random{context.workload().getRNGForThread(RollingCollectionReader::id())},
      _rollingCollectionNames{WorkloadContext::getActorSharedState<RollingCollectionManager,
        RollingCollectionManager::RollingCollectionNames>()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], RollingCollectionReader::id()} {}

namespace {
auto registerRollingCollectionReader = Cast::registerDefault<RollingCollectionReader>();
}  // namespace
}  // namespace genny::actor
