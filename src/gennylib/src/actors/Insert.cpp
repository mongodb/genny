#include <gennylib/actors/Insert.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include "log.hh"
#include <bsoncxx/json.hpp>
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

struct genny::actor::Insert::PhaseConfig {
    mongocxx::collection collection;
    std::unique_ptr<value_generators::DocumentGenerator> json_document;

    PhaseConfig(PhaseContext& phaseContext, std::mt19937_64& rng, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          json_document{value_generators::makeDoc(phaseContext.get("Document"), rng)} {}
};

void genny::actor::Insert::run() {
    for (auto&& [p, h] : _loop) {
        for (const auto&& _ : h) {
            auto op = _insertTimer.raii();
            bsoncxx::builder::stream::document mydoc{};
            auto view = h->json_document->view(mydoc);
            BOOST_LOG_TRIVIAL(info) << " Inserting " << bsoncxx::to_json(view);
            h->collection.insert_one(view);
            _operations.incr();
        }
    }
}


genny::actor::Insert::Insert(genny::ActorContext& context, const unsigned int thread)
    : _rng{context.workload().createRNG()},
      _insertTimer{context.timer("insert", thread)},
      _operations{context.counter("operations", thread)},
      _client{std::move(context.client())},
      _loop{context, _rng, (*_client)[context.get<std::string>("Database")]} {}

genny::ActorVector genny::actor::Insert::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "Insert") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::Insert>(context, i));
    }
    return out;
}
