#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>
#include <yaml-cpp/yaml.h>

#include <gennylib/actors/RunCommand.hpp>
#include <gennylib/value_generators.hpp>
#include <log.hh>

namespace {}

struct genny::actor::RunCommand::RunCommandConfig {
    RunCommandConfig(const OperationContext& operationContext,
                     mongocxx::client& client,
                     std::mt19937_64& rng)
        : database(client[operationContext.get<std::string>("Database")]) {
        documentTemplate = genny::value_generators::makeDoc(operationContext.get("Command"), rng);
    }

    void run() {
        bsoncxx::builder::stream::document document{};
        auto view = documentTemplate->view(document);
        BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view) << " on database: " << database.name();
        database.run_command(view);
    }

    std::unique_ptr<genny::value_generators::DocumentGenerator> documentTemplate;
    mongocxx::database database;
};

struct genny::actor::RunCommand::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                std::mt19937_64& rng,
                mongocxx::pool::entry& client,
                ActorContext& actorContext,
                const unsigned int thread) {
        for (auto&& [metricName, opCtx] : context.operations()) {
            timersAndCommands.emplace_back(
                std::make_pair(actorContext.timer(metricName, thread),
                               std::make_unique<RunCommandConfig>(*opCtx, *client, rng)));
        }
    }

    std::vector<std::pair<metrics::Timer, std::unique_ptr<RunCommandConfig>>> timersAndCommands;
};

void genny::actor::RunCommand::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            for (auto&& [timer, commandOperation] : config->timersAndCommands) {
                auto op = timer.raii();
                commandOperation->run();
            }
        }
    }
}

genny::actor::RunCommand::RunCommand(genny::ActorContext& context, const unsigned int thread)
    : _rng{context.workload().createRNG()},
      _client{context.client()},
      _loop{context, _rng, _client, context, thread} {}

genny::ActorVector genny::actor::RunCommand::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "RunCommand") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::RunCommand>(context, i));
    }
    return out;
}
