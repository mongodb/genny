#include <gennylib/actors/BigUpdate.hpp>

#include <string>
#include <memory>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace {

}  // namespace

struct genny::actor::BigUpdate::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                std::mt19937_64& rng,
                mongocxx::pool::entry& client,
                int thread)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<uint>("CollectionCount")},
          queryDocument{value_generators::makeDoc(context.get("UpdateFilter"), rng)},
          updateDocument{value_generators::makeDoc(context.get("Update"), rng)},
          uniformDistribution{0, numCollections} {}

    mongocxx::database database;
    uint numCollections;
    std::unique_ptr<value_generators::DocumentGenerator> queryDocument;
    std::unique_ptr<value_generators::DocumentGenerator> updateDocument;
    //    std::unique_ptr<value_generators::DocumentGenerator>  updateOptions;

    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<uint> uniformDistribution;

};

void genny::actor::BigUpdate::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // Select a collection
            auto collectionNumber = config->uniformDistribution(_rng);
            auto collectionName = "Collection" + std::to_string(collectionNumber);
            auto collection = config->database[collectionName];

            // Perform update
            bsoncxx::builder::stream::document updateFilter{};
            bsoncxx::builder::stream::document update{};
            auto filterView = config->queryDocument->view(updateFilter);
            auto updateView = config->updateDocument->view(update);
            {
                // Only time the actual update, not the setup of arguments
                auto op = _updateTimer.raii();
                auto result = collection.update_many(filterView, updateView);
                _updateCount.incr(result->modified_count());
            }
        }
    }
}

genny::actor::BigUpdate::BigUpdate(genny::ActorContext& context, const unsigned int thread)
    : _rng{context.workload().createRNG()},
      _updateTimer{context.timer("updateTime", thread)},
      _updateCount{context.counter("updatedDocuments", thread)},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, thread} {}

genny::ActorVector genny::actor::BigUpdate::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "BigUpdate") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::BigUpdate>(context, i));
    }
    return out;
}
