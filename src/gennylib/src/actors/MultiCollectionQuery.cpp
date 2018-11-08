#include <gennylib/actors/MultiCollectionQuery.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace {

}  // namespace

struct genny::actor::MultiCollectionQuery::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                std::mt19937_64& rng,
                mongocxx::pool::entry& client,
                int thread)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<uint>("CollectionCount")},
          filterDocument{value_generators::makeDoc(context.get("Filter"), rng)},
          uniformDistribution{0, numCollections} {
              minDelay = std::chrono::milliseconds(0);
              auto delay = context.get<std::chrono::milliseconds, false>("MinDelay");
              if (delay)
                  minDelay = *delay;
              // Set up the options. All options are optional
              auto limit = context.get<int32_t, false>("Limit");
              if (limit)
                  options.limit(*limit);
}

    mongocxx::database database;
    uint numCollections;
    std::unique_ptr<value_generators::DocumentGenerator> filterDocument;
    // uniform distribution random int for selecting collection
    std::uniform_int_distribution<uint> uniformDistribution;
    std::chrono::milliseconds minDelay;
    mongocxx::options::find options;
};

void genny::actor::MultiCollectionQuery::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            // Take a timestamp -- remove after TIG-1155
            auto startTime = std::chrono::steady_clock::now();

            // Select a collection
            // This area is ripe for defining a collection generator, based off a string generator. It could look like:
            // collection: {@concat: [Collection, @randomint: {min: 0, max: *CollectionCount]}
            // Requires a string concat generator, and a translation of a string to a collection
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
                for (auto&& doc: cursor) {
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

genny::actor::MultiCollectionQuery::MultiCollectionQuery(genny::ActorContext& context, const unsigned int thread)
    : _rng{context.workload().createRNG()},
      _queryTimer{context.timer("queryTime", thread)},
      _documentCount{context.counter("returnedDocuments", thread)},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, thread} {}

genny::ActorVector genny::actor::MultiCollectionQuery::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "MultiCollectionQuery") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::MultiCollectionQuery>(context, i));
    }
    return out;
}
