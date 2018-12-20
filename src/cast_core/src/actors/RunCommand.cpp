#include <cast_core/actors/RunCommand.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>
#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>
#include <gennylib/value_generators.hpp>

namespace genny {

struct actor::RunCommand::Operation {
    struct Config {
        mongocxx::client& client;
        std::mt19937_64& rng;
        ActorId id;

        std::string database;

        const YAML::Node& yaml;
    };

    Operation(const PhaseContext& context, ActorContext& actorContext, Config config_)
        : config{config_}, database{config.client[config.database]} {
        if (!config.yaml.IsMap()) {
            throw InvalidConfigurationException("'Operation' must be of map type.");
        }

        // Only record metrics if a 'MetricsName' field is defined for an operation.
        auto yamlMetricsName = config.yaml["MetricsName"];
        if (yamlMetricsName) {
            timer = std::make_optional<metrics::Timer>(
                actorContext.timer(yamlMetricsName.as<std::string>(), config.id));
        }

        // Build our proper command
        auto yamlCommand = config.yaml["OperationCommand"];
        documentTemplate = value_generators::makeDoc(yamlCommand, config.rng);

        // Check if we're quiet
        isQuiet = context.get<bool, false>("Quiet").value_or(false);
    }

    void run() {
        bsoncxx::builder::stream::document document{};
        auto view = documentTemplate->view(document);
        if (!isQuiet) {
            BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view)
                                    << " on database: " << config.database;
        }

        std::unique_ptr<metrics::RaiiStopwatch> watch;
        if (timer) {
            watch = std::make_unique<metrics::RaiiStopwatch>(timer->raii());
        }

        database.run_command(view);
    }

    Config config;
    mongocxx::database database;

    std::optional<metrics::Timer> timer;
    std::unique_ptr<value_generators::DocumentGenerator> documentTemplate;
    bool isQuiet = false;
};


struct actor::RunCommand::PhaseState {
    PhaseState(PhaseContext& context,
               ActorContext& actorContext,
               std::mt19937_64& rng,
               mongocxx::pool::entry& client,
               ActorId id) {
        auto actorType = context.get<std::string>("Type");
        auto database = context.get<std::string, false>("Database").value_or("admin");
        if (actorType == "AdminCommand" && database != "admin") {
            throw InvalidConfigurationException(
                "AdminCommands can only be run on the 'admin' database.");
        }

        auto addOperation = [&](YAML::Node node) {
            operations.push_back(std::make_unique<Operation>(
                context, actorContext, Operation::Config{*client, rng, id, database, node}));
        };

        auto operationList = context.get<YAML::Node, false>("Operations");
        auto operationUnit = context.get<YAML::Node, false>("Operation");
        if (operationList && operationUnit) {
            throw InvalidConfigurationException(
                "Can't have both 'Operations' and 'Operation' in YAML config.");
        } else if (operationList) {
            if (!operationList->IsSequence()) {
                throw InvalidConfigurationException("'Operations' must be of sequence type.");
            }
            for (auto&& op : *operationList) {
                addOperation(op);
            }
        } else if (operationUnit) {
            addOperation(*operationUnit);
        } else if (!operationUnit && !operationList) {
            throw InvalidConfigurationException("No operations found in RunCommand Actor.");
        }
    }

    std::vector<std::unique_ptr<Operation>> operations;
};

void actor::RunCommand::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            for (auto&& op : config->operations) {
                op->run();
            }
        }
    }
}

actor::RunCommand::RunCommand(ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _client{context.client()},
      _loop{context, context, _rng, _client, RunCommand::id()} {}

namespace {
using AdminCommandProducer = DefaultActorProducer<actor::RunCommand>;

auto registerRunCommand = Cast::registerDefault<actor::RunCommand>();
auto registerAdminCommand =
    Cast::registerCustom(std::make_shared<AdminCommandProducer>("AdminCommand"));
}  // namespace

} // namespace genny
