#include <gennylib/actors/BigQuery.hpp>

#include <gennylib/actors/BigUpdate.hpp>

#include <string>
#include <memory>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace {

}  // namespace

struct genny::actor::BigQuery::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                std::mt19937_64& rng,
                mongocxx::pool::entry& client,
                int thread)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<uint>("CollectionCount")},
          filterDocument{value_generators::makeDoc(context.get("UpdateFilter"), rng)},
          limit{context.get<uint>("Limit")}, // TODO this should be optional
          uniformDistribution{0, numCollections} {}

    mongocxx::database database;
    uint numCollections;
    std::unique_ptr<value_generators::DocumentGenerator> filterDocument;
    uint limit;
    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<uint> uniformDistribution;

};

void genny::actor::BigQuery::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // Select a collection
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
                auto cursor = collection.find(filterView);
                // exhaust the cursor
                //_documentCount.incr(result->modified_count());
            }
        }
    }
}

genny::actor::BigQuery::BigQuery(genny::ActorContext& context, const unsigned int thread)
    : _rng{context.workload().createRNG()},
      _queryTimer{context.timer("queryTime", thread)},
      _documentCount{context.counter("returnedDocuments", thread)},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, thread} {}

genny::ActorVector genny::actor::BigQuery::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "BigQuery") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::BigQuery>(context, i));
    }
    return out;
}
