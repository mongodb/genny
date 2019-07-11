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

#include <cast_core/actors/RandomSampler.hpp>
#include <cast_core/actors/CollectionScanner.hpp>
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

struct RandomSampler::PhaseConfig {
    mongocxx::database database;
    std::vector<std::string> collectionNames;
    genny::DefaultRandom random;
    std::uniform_int_distribution<> integerDistribution;
    metrics::Operation readOperation;
    metrics::Operation readWithScanOperation;
    PhaseConfig(PhaseContext& context, const mongocxx::database& db, ActorId id, int collectionCount,
        int threads, ActorCounter &counter)
        : database{db}, readOperation{context.operation("Read", id)},
        readWithScanOperation{context.operation("ReadWithScan", id)} {
        int actorId = counter;
        counter ++;
        // Distribute the collections among the actors.
        collectionNames = CollectionScanner::getCollectionNames(collectionCount, threads, actorId);
        // Setup the int distribution.
        integerDistribution = std::uniform_int_distribution(0, (int)collectionNames.size() - 1);
    }
};

void RandomSampler::run() {
    for (auto&& config : _loop) {
        // Construct basic pipeline for retrieving one random record.
        mongocxx::pipeline pipeline{};
        pipeline.sample(1);
        for (const auto&& _ : config) {
            try {
                auto statTracker = _collectionScannerCounter > 0 ? config->readWithScanOperation.start() :
                  config->readOperation.start();
                if (_collectionScannerCounter > 0){
                    std::cout << "Collection scanner running" << std::endl;
                }
                int index = config->collectionNames.size() > 1 ? config->integerDistribution(config->random) : 0;
                auto cursor = config->database[config->collectionNames[index]].aggregate(pipeline,
                    mongocxx::options::aggregate{});
                for (auto doc : cursor){
                  ;
                }
                statTracker.success();
            } catch(mongocxx::operation_exception& e) {
                //BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            }
        }
    }
}

RandomSampler::RandomSampler(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _actorCounter{WorkloadContext::getActorSharedState<RandomSampler, ActorCounter>()},
      _collectionScannerCounter{WorkloadContext::getActorSharedState<CollectionScanner,
        CollectionScanner::RunningActorCounter>()},
      _loop{
        context,
        (*_client)[context["Database"].to<std::string>()],
        RandomSampler::id(),
        context["CollectionCount"].to<IntegerSpec>(),
        context["Threads"].to<IntegerSpec>(),
        _actorCounter
      } { }

namespace {
auto registerRandomSampler = Cast::registerDefault<RandomSampler>();
}
}
