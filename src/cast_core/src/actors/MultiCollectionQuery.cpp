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

#include <cast_core/actors/MultiCollectionQuery.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace genny::actor {

/** @private */
struct MultiCollectionQuery::PhaseConfig {
    PhaseConfig(PhaseContext& context, genny::DefaultRandom& rng, mongocxx::pool::entry& client)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<UIntSpec, true>("CollectionCount")},
          filterDocument{value_generators::makeDoc(context.get("Filter"), rng)},
          uniformDistribution{0, numCollections},
          minDelay{context.get<TimeSpec, false>("MinDelay").value_or(genny::TimeSpec(0))} {}

    mongocxx::database database;
    size_t numCollections;
    std::unique_ptr<value_generators::DocumentGenerator> filterDocument;
    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<size_t> uniformDistribution;
    std::chrono::milliseconds minDelay;
    mongocxx::options::find options;
};

void MultiCollectionQuery::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            // Take a timestamp -- remove after TIG-1155
            auto startTime = std::chrono::steady_clock::now();

            // Select a collection
            // This area is ripe for defining a collection generator, based off a string generator.
            // It could look like: collection: {^Concat: [Collection, ^RandomInt: {min: 0, max:
            // *CollectionCount]} Requires a string concat generator, and a translation of a string
            // to a collection
            auto collectionNumber = config->uniformDistribution(_rng);
            auto collectionName = "Collection" + std::to_string(collectionNumber);
            auto collection = config->database[collectionName];

            // Perform a query
            bsoncxx::builder::stream::document filter{};
            auto filterView = config->filterDocument->view(filter);
            // BOOST_LOG_TRIVIAL(info) << "Filter is " <<  bsoncxx::to_json(filterView);
            // BOOST_LOG_TRIVIAL(info) << "Collection Name is " << collectionName;
            {
                // Only time the actual update, not the setup of arguments
                auto op = _queryTimer.raii();
                auto cursor = collection.find(filterView, config->options);
                // exhaust the cursor
                uint count = 0;
                for (auto&& doc : cursor) {
                    doc.length();
                    count++;
                }
                _documentCount.incr(count);
            }
            // make sure enough time has passed. Sleep if needed -- remove after TIG-1155
            auto elapsedTime = std::chrono::steady_clock::now() - startTime;
            if (elapsedTime < config->minDelay)
                std::this_thread::sleep_for(config->minDelay - elapsedTime);
        }
    }
}

MultiCollectionQuery::MultiCollectionQuery(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _queryTimer{context.timer("queryTime", MultiCollectionQuery::id())},
      _documentCount{context.counter("returnedDocuments", MultiCollectionQuery::id())},
      _client{context.client()},
      _loop{context, _rng, _client} {}

namespace {
auto registerMultiCollectionQuery =
    genny::Cast::registerDefault<genny::actor::MultiCollectionQuery>();
}
}  // namespace genny::actor
