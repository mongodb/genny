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

#include <memory>
#include <random>
#include <math.h>

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
    std::vector<mongocxx::collection> collections;
    std::mt19937 random;
    std::uniform_int_distribution<> integerDistribution;
    TimeSpec interval;
    bool hasInterval = false;
    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db, ActorId id, int threads, ActorCounter &counter)
        : database{db},
          interval{phaseContext["Interval"].maybe<TimeSpec>().value_or(TimeSpec{})} {
        // Construct interval and set flag
        if (interval.value != std::chrono::nanoseconds::zero()){
            hasInterval = true;
        }
        int actorId = counter;
        counter ++;

        // Distribute the collections among the actors "fairly"
        auto collectionNames = database.list_collection_names();
        int collectionsPerActor = std::round(collectionNames.size() / (double)threads);
        if (collectionsPerActor == 0) collectionsPerActor = 1;
        int collectionIndexStart = ((actorId % collectionNames.size()) * collectionsPerActor) % collectionNames.size();
        int collectionIndexEnd = collectionIndexStart + collectionsPerActor;
        for (int i = collectionIndexStart; i < collectionIndexEnd; ++i){
            if (i >= collectionNames.size()) {
                collectionIndexEnd %= collectionNames.size();
                i = 0;
            }
            collections.push_back(database.collection(collectionNames[i]));
        }

        //If we are off due to a rounding error
        if (collectionIndexStart + collectionsPerActor < collectionNames.size() && actorId == threads - 1){
            for (auto i = collectionIndexStart + collectionsPerActor; i < collectionNames.size(); ++ i){
                collections.push_back(database.collection(collectionNames[i]));
                collectionsPerActor ++;
            }
        }
        // Setup the random distribution
        auto start = std::chrono::system_clock::now();
        random.seed(start.time_since_epoch().count());
        integerDistribution = std::uniform_int_distribution(0, (int)collectionsPerActor - 1);
    }
};

void RandomSampler::run() {
    for (auto&& config : _loop) {
        mongocxx::pipeline pipeline{};
        pipeline.sample(1);
        for (const auto&& _ : config) {
            try {
                if (config->hasInterval) {
                    std::this_thread::sleep_for(config->interval.value);
                }
                int chosenspot = config->integerDistribution(config->random);
                if (chosenspot > config->collections.size()) assert(false);
                auto cursor = config->collections[chosenspot].aggregate(pipeline,
                  mongocxx::options::aggregate{});
                for (auto doc : cursor){
                    ;
                }
            } catch(mongocxx::operation_exception& e) {
                //BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            }
        }
    }
}

RandomSampler::RandomSampler(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", RandomSampler::id())},
      _client{context.client()},
      _actorCounter{WorkloadContext::getActorSharedState<RandomSampler, ActorCounter>()},
      _loop{
        context,
        (*_client)[context["Database"].to<std::string>()],
        RandomSampler::id(),
        context["Threads"].to<int>(),
        _actorCounter
      } { }

namespace {
auto registerRandomSampler = Cast::registerDefault<RandomSampler>();
}
}
