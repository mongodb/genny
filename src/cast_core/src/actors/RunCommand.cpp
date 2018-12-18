#include <cast_core/actors/RunCommand.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>
#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>
#include <gennylib/value_generators.hpp>

using namespace std;

struct genny::actor::RunCommand::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                mt19937_64& rng,
                mongocxx::pool::entry& client,
                ActorContext& actorContext,
                int id) {
        auto actorType = context.get<string>("Type");
        auto database = context.get<string, false>("Database");
        if (actorType == "AdminCommand" && (database && database != "admin")) {
            throw InvalidConfigurationException(
                "AdminCommands can only be run on the 'admin' database.");
        }
        auto operationList = context.get<YAML::Node, false>("Operations");
        auto operationUnit = context.get<YAML::Node, false>("Operation");
        if (operationList && operationUnit) {
            throw InvalidConfigurationException(
                "Can't have both 'Operations' and 'Operation' in YAML config.");
        }
        if (operationList) {
            assert(operationList->IsSequence());
            for (auto&& op : *operationList) {
                addOperation(op, context, rng, client, actorContext, id);
            }
        } else if (operationUnit) {
            assert(operationUnit->IsMap());
            addOperation(*operationUnit, context, rng, client, actorContext, id);
        }
    }

    struct RunCommandConfig {
        RunCommandConfig(const PhaseContext& context,
                         mongocxx::client& client,
                         mt19937_64& rng,
                         const YAML::Node& commandNode,
                         optional<metrics::Timer> timer)
            : database{client[context.get<string>("Database")]},
              documentTemplate{genny::value_generators::makeDoc(commandNode, rng)},
              timer{timer} {}

        void run() {
            bsoncxx::builder::stream::document document{};
            auto view = documentTemplate->view(document);
            BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view)
                                    << " on database: " << database.name();
            {
                if (timer) {
                    // The raii version of the timer only exists in this scope.
                    auto op = timer->raii();
                    database.run_command(view);
                } else {
                    database.run_command(view);
                }
            }
        }

        optional<metrics::Timer> timer;
        unique_ptr<genny::value_generators::DocumentGenerator> documentTemplate;
        mongocxx::database database;
    };

    void addOperation(const YAML::Node& operation,
                      PhaseContext& context,
                      mt19937_64& rng,
                      mongocxx::pool::entry& client,
                      ActorContext& actorContext,
                      int id) {
        if (operation["OperationName"].as<string>() != "RunCommand")
            return;
        // Only record metrics if a 'MetricsName' field is defined for an operation.
        optional<metrics::Timer> timer = operation["MetricsName"]
            ? optional<metrics::Timer>{actorContext.timer(operation["MetricsName"].as<string>(),
                                                          id)}
            : nullopt;
        auto commandNode = operation["OperationCommand"];
        runCommandConfigs.emplace_back(
            make_unique<RunCommandConfig>(context, *client, rng, commandNode, timer));
    }

    vector<unique_ptr<RunCommandConfig>> runCommandConfigs;
};

void genny::actor::RunCommand::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            for (auto&& runCommand : config->runCommandConfigs) {
                runCommand->run();
            }
        }
    }
}

genny::actor::RunCommand::RunCommand(genny::ActorContext& context, const unsigned int threads)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _client{context.client()},
      _loop{context, _rng, _client, context, threads} {}

class RunCommandProducer : public genny::ActorProducer {
public:
    RunCommandProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context.get<std::string>("Type") != "RunCommand") {
            return {};
        }
        genny::ActorVector out;
        for (uint i = 0; i < context.get<int>("Threads"); ++i) {
            out.emplace_back(std::make_unique<genny::actor::RunCommand>(context, i));
        }
        return out;
    }
};

class AdminCommandProducer : public genny::ActorProducer {
public:
    AdminCommandProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context.get<std::string>("Type") != "AdminCommand") {
            return {};
        }
        genny::ActorVector out;
        for (uint i = 0; i < context.get<int>("Threads"); ++i) {
            out.emplace_back(std::make_unique<genny::actor::RunCommand>(context, i));
        }
        return out;
    }
};

namespace {
std::shared_ptr<genny::ActorProducer> runCommandProducer =
    std::make_shared<RunCommandProducer>("RunCommand");
std::shared_ptr<genny::ActorProducer> adminCommandProducer =
    std::make_shared<AdminCommandProducer>("AdminCommand");
auto registerRunCommand = genny::Cast::registerCustom<genny::ActorProducer>(runCommandProducer);
auto registerAdminCommand = genny::Cast::registerCustom<genny::ActorProducer>(adminCommandProducer);
}  // namespace
