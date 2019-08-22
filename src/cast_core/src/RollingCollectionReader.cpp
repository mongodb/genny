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
#include <cast_core/actors/RollingCollectionWriter.hpp>
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
    mongocxx::database database;
    RollingCollectionWindow rollingCollectionWindow;
    DocumentGenerator filterExpr;
    double distribution;
    boost::random::uniform_real_distribution<double> realDistribution;
    metrics::Operation findOperation;
    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id, int64_t collectionWindowSize)
        : rollingCollectionWindow{collectionWindowSize},
          database{db},
          filterExpr{phaseContext["Filter"].to<DocumentGenerator>(phaseContext, id)},
          distribution{phaseContext["Distribution"].maybe<double>().value_or(0)},
          findOperation{phaseContext.operation("Find", id)} {
        BOOST_LOG_TRIVIAL(info) << "Window size: " << collectionWindowSize;
        // Setup the real distribution.
        realDistribution =
            boost::random::uniform_real_distribution<double>(0, 1);
    }
};

int getNextCollectionId(RollingCollectionWindow window, double distribution, double rand){
    return std::floor((window.max - ((distribution * rand) * window.max)) + window.min);
}

void RollingCollectionReader::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            updateCurrentIdWindow(config->rollingCollectionWindow);
            auto name = getRollingCollectionName(getNextCollectionId(config->rollingCollectionWindow,
              config->distribution, config->realDistribution(_random)));
            if (!config->database.has_collection(name)) {
                /*
                 * Hopefully this is a result of the rolling collection manager
                 * deleting a collection we were about to use.
                 */
                BOOST_LOG_TRIVIAL(info) << "Missing collection: " << name;
            } else {
                auto statTracker = config->findOperation.start();
                try {
                    BOOST_LOG_TRIVIAL(info) << "Reading collection: " << name;
                    BOOST_LOG_TRIVIAL(info) << "Window size: " << config->rollingCollectionWindow.min
                     << " -> " << config->rollingCollectionWindow.max;
                    auto optionalDocument = config->database[name].find_one(config->filterExpr().view());
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
                    BOOST_LOG_TRIVIAL(info) << "Exception:";
                    statTracker.failure();
                }
            }
        }
    }
}

RollingCollectionReader::RollingCollectionReader(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _collectionWindowSize{context["CollectionWindowSize"].maybe<IntegerSpec>().value_or(0)},
      _random{context.workload().getRNGForThread(RollingCollectionReader::id())},
      _loop{context, (*_client)[context["Database"].to<std::string>()], RollingCollectionReader::id(),
        context["CollectionWindowSize"].maybe<IntegerSpec>().value_or(0)} {}

namespace {
auto registerRollingCollectionReader = Cast::registerDefault<RollingCollectionReader>();
}  // namespace
}  // namespace genny::actor
