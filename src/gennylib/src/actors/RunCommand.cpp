#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>
#include <yaml-cpp/yaml.h>

#include <gennylib/actors/RunCommand.hpp>
#include <gennylib/value_generators.hpp>
#include <log.hh>

using namespace std;

namespace {}

struct genny::actor::RunCommand::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                mt19937_64& rng,
                mongocxx::pool::entry& client,
                ActorContext& actorContext,
                const unsigned int thread) {
        auto operations = context.get<YAML::Node, false>("Operations");
        // Only record metrics if a 'MetricsName' field is defined for an operation.
        optional<metrics::Timer> timer;
        YAML::Node commandNode;
        if (operations) {
            for (auto&& op : *operations) {
                if (op["Name"].as<string>() != "RunCommand") continue;
                timer = op["MetricsName"] ? optional<metrics::Timer>{actorContext.timer(
                                                op["MetricsName"].as<string>(), thread)}
                                          : nullopt;
                commandNode = op["Command"];
                timersAndCommands.emplace_back(make_pair(
                    timer, make_unique<RunCommandConfig>(context, *client, rng, commandNode)));
            }
        } else if (auto op = context.get<YAML::Node, false>("Operation")) {
            if (op->as<string>() != "RunCommand") return;
            auto metricsName = context.get<YAML::Node, false>("MetricsName");
            commandNode = context.get<YAML::Node>("Command");
            timer = metricsName
                ? optional<metrics::Timer>{actorContext.timer(metricsName->as<string>(), thread)}
                : nullopt;
            timersAndCommands.emplace_back(make_pair(
                timer, make_unique<RunCommandConfig>(context, *client, rng, commandNode)));
        }
    }

    struct RunCommandConfig {
        RunCommandConfig(const PhaseContext& context,
                         mongocxx::client& client,
                         mt19937_64& rng,
                         const YAML::Node& commandNode)
            : database{client[context.get<string>("Database")]},
              documentTemplate{genny::value_generators::makeDoc(commandNode, rng)} {}

        void run(optional<metrics::Timer> timer) {
            bsoncxx::builder::stream::document document{};
            auto view = documentTemplate->view(document);
            BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view)
                                    << " on database: " << database.name();
            {
                if (timer)
                    auto op = timer->raii();
                database.run_command(view);
            }
        }

        unique_ptr<genny::value_generators::DocumentGenerator> documentTemplate;
        mongocxx::database database;
    };

    vector<pair<optional<metrics::Timer>, unique_ptr<RunCommandConfig>>> timersAndCommands;
};

void genny::actor::RunCommand::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            for (auto&& [timer, commandOperation] : config->timersAndCommands) {
                commandOperation->run(timer);
            }
        }
    }
}

genny::actor::RunCommand::RunCommand(genny::ActorContext& context, const unsigned int thread)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _client{context.client()},
      _loop{context, _rng, _client, context, thread} {}

genny::ActorVector genny::actor::RunCommand::producer(genny::ActorContext& context) {
    auto out = vector<unique_ptr<genny::Actor>>{};
    if (context.get<string>("Type") != "RunCommand") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(make_unique<genny::actor::RunCommand>(context, i));
    }
    return out;
}
