#include <cast_core/actors/MultiCollectionUpdate.hpp>

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

namespace {}  // namespace

struct genny::actor::MultiCollectionUpdate::PhaseConfig {
    PhaseConfig(PhaseContext& context, std::mt19937_64& rng, mongocxx::pool::entry& client)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<uint>("CollectionCount")},
          queryDocument{value_generators::makeDoc(context.get("UpdateFilter"), rng)},
          updateDocument{value_generators::makeDoc(context.get("Update"), rng)},
          uniformDistribution{0, numCollections},
          minDelay{context.get<std::chrono::milliseconds, false>("MinDelay")
                       .value_or(std::chrono::milliseconds(0))} {}

    mongocxx::database database;
    uint numCollections;
    std::unique_ptr<value_generators::DocumentGenerator> queryDocument;
    std::unique_ptr<value_generators::DocumentGenerator> updateDocument;
    // TODO: Enable passing in update options.
    //    std::unique_ptr<value_generators::DocumentGenerator>  updateOptions;

    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<uint> uniformDistribution;
    std::chrono::milliseconds minDelay;
};

void genny::actor::MultiCollectionUpdate::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // Take a timestamp -- remove after TIG-1155
            auto startTime = std::chrono::steady_clock::now();

            // Select a collection
            auto collectionNumber = config->uniformDistribution(_rng);
            auto collectionName = "Collection" + std::to_string(collectionNumber);
            auto collection = config->database[collectionName];

            // Perform update
            bsoncxx::builder::stream::document updateFilter{};
            bsoncxx::builder::stream::document update{};
            auto filterView = config->queryDocument->view(updateFilter);
            auto updateView = config->updateDocument->view(update);
            // BOOST_LOG_TRIVIAL(info) << "Filter is " <<  bsoncxx::to_json(filterView);
            // BOOST_LOG_TRIVIAL(info) << "Update is " << bsoncxx::to_json(updateView);
            // BOOST_LOG_TRIVIAL(info) << "Collection Name is " << collectionName;
            {
                // Only time the actual update, not the setup of arguments
                auto op = _updateTimer.raii();
                auto result = collection.update_many(filterView, updateView);
                _updateCount.incr(result->modified_count());
            }
            // make sure enough time has passed. Sleep if needed -- remove after TIG-1155
            auto elapsedTime = std::chrono::steady_clock::now() - startTime;
            if (elapsedTime < config->minDelay)
                std::this_thread::sleep_for(config->minDelay - elapsedTime);
        }
    }
}

genny::actor::MultiCollectionUpdate::MultiCollectionUpdate(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _updateTimer{context.timer("updateTime", MultiCollectionUpdate::id())},
      _updateCount{context.counter("updatedDocuments", MultiCollectionUpdate::id())},
      _client{context.client()},
      _loop{context, _rng, _client} {}

namespace {
auto registerMultiCollectionUpdate =
    genny::Cast::makeDefaultRegistration<genny::actor::MultiCollectionUpdate>();
}
