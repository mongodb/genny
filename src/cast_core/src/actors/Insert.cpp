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

namespace genny::actor {

struct Insert::PhaseConfig {
    mongocxx::collection collection;
    std::unique_ptr<value_generators::DocumentGenerator> json_document;
    ExecutionStrategy::RunOptions options;

    PhaseConfig(PhaseContext& phaseContext, std::mt19937_64& rng, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          json_document{value_generators::makeDoc(phaseContext.get("Document"), rng)},
          options{phaseContext.get<size_t, false>("Retries").value_or(0)} {}
};

void Insert::run() {
    for (auto&& [phase, config] : _loop) {
        for (const auto&& _ : config) {
            const auto fun = [&]() {
                bsoncxx::builder::stream::document mydoc{};
                auto view = config->json_document->view(mydoc);
                BOOST_LOG_TRIVIAL(info) << " Inserting " << bsoncxx::to_json(view);
                config->collection.insert_one(view);
            };

            _strategy.tryToRun(fun, config->options);
        }

        _strategy.recordMetrics();
    }
}

Insert::Insert(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _strategy{context, "insert"},
      _client{std::move(context.client())},
      _loop{context, _rng, (*_client)[context.get<std::string>("Database")]} {}

namespace {
auto registerInsert = genny::Cast::registerDefault<genny::actor::Insert>();
}
}  // namespace genny::actor
