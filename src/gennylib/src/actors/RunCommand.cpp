#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/actors/RunCommand.hpp>
#include <gennylib/operations/RunCommand.hpp>
#include <gennylib/value_generators.hpp>
#include <log.hh>

namespace {}

struct genny::actor::RunCommand::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                std::mt19937_64& rng,
                mongocxx::pool::entry& client,
                ActorContext& actorContext,
                const unsigned int thread)
        : database{(*client)[context.get<std::string>("Database")]} {
        for (auto&& opContext : context.operations()) {
            timers.push_back(actorContext.timer(opContext.first, thread));
            operations.push_back(
                std::make_unique<genny::operation::RunCommand>(*(opContext.second), *client, rng));
        }
    }

    mongocxx::database database;
    std::vector<std::unique_ptr<genny::operation::RunCommand>> operations;
    std::vector<metrics::Timer> timers;
};

void genny::actor::RunCommand::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            for (size_t opIndex = 0; opIndex < config->operations.size(); opIndex++) {
                auto op = config->timers[opIndex].raii();
                config->operations[opIndex]->run();
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
