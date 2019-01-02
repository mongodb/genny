#include <cast_core/actors/RunCommand.hpp>

#include <chrono>
#include <thread>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>

#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/value_generators.hpp>

namespace genny {

class actor::RunCommand::Operation {
public:
    struct Context {
        PhaseContext& phaseContext;
        ActorContext& actorContext;

        ActorId id;
        std::string databaseName;
        mongocxx::database database;
    };

    using Options = config::OperationOptions;

public:
    Operation(Context context,
              std::unique_ptr<value_generators::DocumentGenerator> docTemplate,
              Options opts)
        : _database{context.database}, _doc{std::move(docTemplate)}, _options{opts} {
        // Only record metrics if we have a name for the operation.
        if (!_options.metricsName.empty()) {
            _timer = std::make_optional<metrics::Timer>(
                context.actorContext.timer(_options.metricsName, context.id));
        }
    }

    void run() {
        bsoncxx::builder::stream::document document{};
        auto view = _doc->view(document);
        if (!_options.isQuiet) {
            BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view)
                                    << " on database: " << _database.name().data();
        }

        if (_options.preDelayMS >= std::chrono::milliseconds{0}) {
            std::this_thread::sleep_for(_options.preDelayMS);
        }

        auto watch = [&]() {
            if (!_timer)
                return std::unique_ptr<metrics::RaiiStopwatch>{};

            return std::make_unique<metrics::RaiiStopwatch>(_timer->raii());
        }();

        _database.run_command(view);

        watch.reset();

        if (_options.postDelayMS >= std::chrono::milliseconds{0}) {
            std::this_thread::sleep_for(_options.postDelayMS);
        }
    }

private:
    mongocxx::database _database;
    std::unique_ptr<value_generators::DocumentGenerator> _doc;
    Options _options;

    std::optional<metrics::Timer> _timer;
};


struct actor::RunCommand::PhaseState {
    PhaseState(PhaseContext& context,
               ActorContext& actorContext,
               std::mt19937_64& rng,
               mongocxx::pool::entry& client,
               ActorId id)
        : strategy{actorContext, id, "RunCommand"} {
        auto actorType = context.get<std::string>("Type");
        auto database = context.get<std::string, false>("Database").value_or("admin");
        if (actorType == "AdminCommand" && database != "admin") {
            throw InvalidConfigurationException(
                "AdminCommands can only be run on the 'admin' database.");
        }

        auto opContext = Operation::Context{
            context, actorContext, id, database, (*client)[database],
        };

        auto addOperation = [&](YAML::Node node) {
            auto yamlCommand = node["OperationCommand"];
            auto doc = value_generators::makeDoc(yamlCommand, rng);

            auto options = node.as<Operation::Options>(Operation::Options{});
            operations.push_back(std::make_unique<Operation>(opContext, std::move(doc), options));
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

    ExecutionStrategy strategy;
    std::vector<std::unique_ptr<Operation>> operations;
};

void genny::actor::RunCommand::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            config->strategy.run([&]() {
                for (auto&& op : config->operations) {
                    op->run();
                }
            });
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

}  // namespace genny
