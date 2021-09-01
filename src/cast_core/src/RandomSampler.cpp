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
    std::vector<mongocxx::collection> collections;
    boost::random::uniform_int_distribution<> integerDistribution;
    /*
     * Two separate trackers as we want to be able to observe the impact
     * of the collection scanner on the read throughput.
     */
    metrics::Operation readOperation;
    metrics::Operation readWithScanOperation;
    mongocxx::pipeline pipeline{};

    PhaseConfig(PhaseContext& context,
                const RandomSampler* actor,
                const mongocxx::database& db,
                int collectionCount,
                int threads)
        : readOperation{context.operation("Read", actor->id())},
          readWithScanOperation{context.operation("ReadWithScan", actor->id())} {
        // Construct basic pipeline for retrieving 10 random records.
        pipeline.sample(10);

        // Distribute the collections among the actors.
        for (const auto& collectionName :
             distributeCollectionNames(collectionCount, threads, actor->_index)) {
            collections.push_back(db[collectionName]);
        }
        // Setup the int distribution.
        integerDistribution =
            boost::random::uniform_int_distribution(0, (int)collections.size() - 1);
    }
};

void RandomSampler::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto statTracker = _activeCollectionScannerInstances > 0
                ? config->readWithScanOperation.start()
                : config->readOperation.start();
            int index = config->collections.size() > 1 ? config->integerDistribution(_random) : 0;
            auto cursor = config->collections[index].aggregate(config->pipeline,
                                                               mongocxx::options::aggregate{});
            for (auto doc : cursor) {
                statTracker.addDocuments(1);
                statTracker.addBytes(doc.length());
            }
            statTracker.success();
        }
    }
}

RandomSampler::RandomSampler(genny::ActorContext& context)
    : Actor{context},
      _client{context.client(v1::DEFAULT_CLIENT_NAME, RandomSampler::id())},
      _index{WorkloadContext::getActorSharedState<RandomSampler, ActorCounter>().fetch_add(1)},
      _activeCollectionScannerInstances{
          WorkloadContext::getActorSharedState<CollectionScanner,
                                               CollectionScanner::RunningActorCounter>()},
      _random{context.workload().getRNGForThread(RandomSampler::id())},
      _loop{context,
            this,
            (*_client)[context["Database"].to<std::string>()],
            context["CollectionCount"].to<IntegerSpec>(),
            context["Threads"].to<IntegerSpec>()} {}

namespace {
auto registerRandomSampler = Cast::registerDefault<RandomSampler>();
}
}  // namespace genny::actor
