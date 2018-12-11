#include <cast_core/actors/Insert.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <bsoncxx/json.hpp>
#include <gennylib/Cast.hpp>
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
    for (auto&& [phase, config] : _loop) {
        for (const auto&& _ : config) {
            auto op = _insertTimer.raii();
            bsoncxx::builder::stream::document mydoc{};
            auto view = config->json_document->view(mydoc);
            BOOST_LOG_TRIVIAL(info) << " Inserting " << bsoncxx::to_json(view);
            config->collection.insert_one(view);
            _operations.incr();
        }
    }
}

genny::actor::Insert::Insert(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _insertTimer{context.timer("insert", Insert::id())},
      _operations{context.counter("operations", Insert::id())},
      _client{std::move(context.client())},
      _loop{context, _rng, (*_client)[context.get<std::string>("Database")]} {}

namespace {
auto registerInsert = genny::Cast::makeDefaultRegistration<genny::actor::Insert>();
}
