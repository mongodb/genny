// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cast_core/actors/RunCommand.hpp>

#include <chrono>
#include <thread>

#include <boost/throw_exception.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/MongoException.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace {
/**
 * @private
 * @param exception exception received from mongocxx::database::run_command()
 * @return if the exception is a result of a connection error or timeout
 */
bool isNetworkError(mongocxx::operation_exception& exception) {
    auto what = std::string{exception.what()};
    return what.find("socket error or timeout") != std::string::npos;
}

/**
 * @private
 * BSON document that when given to run_command requires stepdown
 * to be completed.
 */
bsoncxx::document::value commandRequiringStepdownCompleted = []() {
    auto builder = bsoncxx::builder::stream::document{};
    // `{ "collStats": "__genny-arbitrary-collection" }`
    // NB the actual collection doesn't need to exist, so just pick an arbitrary name that
    // likely doesn't exist.
    auto request = builder << "collStats"
                           << "__genny-arbitrary-collection" << bsoncxx::builder::stream::finalize;
    return request;
}();

/**
 * Runs the given command, expects a timeout, and then awaits all replset
 * stepdowns to complete.
 *
 * @private
 * @param database database on which to run `run_command()`.
 * @param command the command to run
 */
void runThenAwaitStepdown(mongocxx::database& database, bsoncxx::document::view& command) {
    try {
        database.run_command(command);
        throw std::logic_error("Stepdown should throw operation exception");
    } catch (mongocxx::operation_exception& exception) {
        if (!isNetworkError(exception)) {
            throw;
        }
        BOOST_LOG_TRIVIAL(info) << "Post stepdown, running "
                                << bsoncxx::to_json(commandRequiringStepdownCompleted.view());
        try {
            // don't care about the return value
            database.run_command(commandRequiringStepdownCompleted.view());
        } catch (mongocxx::operation_exception& statsException) {
            throw;
        }
    }
}

struct RunCommandOperationConfig {
    /** Default values for each of the keys */
    struct Defaults {
        static constexpr auto kMetricsName = "";
        static constexpr auto kIsQuiet = false;
        static constexpr auto kAwaitStepdown = false;
    };

    /** YAML keys */
    struct Keys {
        static constexpr auto kAwaitStepdown = "OperationAwaitStepdown";
        static constexpr auto kMetricsName = "OperationMetricsName";
        static constexpr auto kIsQuiet = "OperationIsQuiet";
        static constexpr auto kMinPeriod = "OperationMinPeriod";
        static constexpr auto kPreSleep = "OperationSleepBefore";
        static constexpr auto kPostSleep = "OperationSleepAfter";
    };

    std::string metricsName = Defaults::kMetricsName;
    bool isQuiet = Defaults::kIsQuiet;
    bool awaitStepdown = Defaults::kAwaitStepdown;
};

}  // namespace

namespace YAML {

template <>
struct convert<RunCommandOperationConfig> {
    using Config = RunCommandOperationConfig;
    using Defaults = typename Config::Defaults;
    using Keys = typename Config::Keys;

    static Node encode(const Config& rhs) {
        Node node;

        // If we don't have a MetricsName, this key is null
        node[Keys::kMetricsName] = rhs.metricsName;

        node[Keys::kIsQuiet] = rhs.isQuiet;
        node[Keys::kAwaitStepdown] = rhs.awaitStepdown;

        return node;
    }

    static bool decode(const Node& node, Config& rhs) {
        if (!node.IsMap()) {
            return false;
        }

        genny::decodeNodeInto(rhs.metricsName, node[Keys::kMetricsName], Defaults::kMetricsName);
        genny::decodeNodeInto(rhs.isQuiet, node[Keys::kIsQuiet], Defaults::kIsQuiet);
        genny::decodeNodeInto(
            rhs.awaitStepdown, node[Keys::kAwaitStepdown], Defaults::kAwaitStepdown);

        return true;
    }
};

}  // namespace YAML


namespace genny {

/** @private */
class DatabaseOperation {
public:
    using OpConfig = RunCommandOperationConfig;

public:
    DatabaseOperation(PhaseContext& phaseContext,
                      ActorContext& actorContext,
                      ActorId id,
                      const std::string& databaseName,
                      mongocxx::database database,
                      DocumentGenerator commandExpr,
                      OpConfig opts)
        : _databaseName{databaseName},
          _database{std::move(database)},
          _commandExpr{std::move(commandExpr)},
          _options{std::move(opts)},
          _awaitStepdown{opts.awaitStepdown},
          // Only record metrics if we have a name for the operation.
          _operation{!_options.metricsName.empty()
                         ? std::make_optional<metrics::Operation>(
                               actorContext.operation(_options.metricsName, id))
                         : std::nullopt} {}

    static std::unique_ptr<DatabaseOperation> create(YAML::Node node,
                                                     PhaseContext& context,
                                                     ActorContext& actorContext,
                                                     ActorId id,
                                                     mongocxx::pool::entry& client,
                                                     const std::string& database) {
        auto yamlCommand = node["OperationCommand"];
        auto commandExpr = context.createDocumentGenerator(id, yamlCommand);

        auto options = node.as<DatabaseOperation::OpConfig>(DatabaseOperation::OpConfig{});
        return std::make_unique<DatabaseOperation>(context,
                                                   actorContext,
                                                   id,
                                                   database,
                                                   (*client)[database],
                                                   std::move(commandExpr),
                                                   options);
    };

    void run() {
        auto command = _commandExpr();
        auto view = command.view();

        if (!_options.isQuiet) {
            BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view)
                                    << " on database: " << _databaseName;
        }

        // If we have an operation, then we have a watch
        std::optional<metrics::OperationContext> maybeWatch =
            _operation ? std::make_optional(std::move(_operation->start())) : std::nullopt;

        try {
            if (_options.awaitStepdown) {
                runThenAwaitStepdown(_database, view);
                if (maybeWatch) {
                    maybeWatch->success();
                }
            } else {
                _database.run_command(view);
                if (maybeWatch) {
                    maybeWatch->success();
                }
            }
        } catch (mongocxx::operation_exception& e) {
            if (maybeWatch) {
                maybeWatch->discard();
            }
            BOOST_THROW_EXCEPTION(MongoException(e, view));
        }
    }


private:
    std::string _databaseName;
    mongocxx::database _database;
    DocumentGenerator _commandExpr;
    OpConfig _options;

    std::optional<metrics::Operation> _operation;
    bool _awaitStepdown;
};

/** @private */
struct actor::RunCommand::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                ActorContext& actorContext,
                mongocxx::pool::entry& client,
                ActorId id)
        : strategy{actorContext.operation("RunCommand", id)},
          options{context.get<ExecutionStrategy::RunOptions,false>("ExecutionsStrategy").value_or(ExecutionStrategy::RunOptions{})} {
        auto actorType = context.get<std::string>("Type");
        auto database = context.get<std::string, false>("Database").value_or("admin");
        if (actorType == "AdminCommand" && database != "admin") {
            throw InvalidConfigurationException(
                "AdminCommands can only be run on the 'admin' database.");
        }

        auto createOperation = [&](YAML::Node node) {
            return DatabaseOperation::create(node, context, actorContext, id, client, database);
        };

        operations = context.getPlural<std::unique_ptr<DatabaseOperation>>(
            "Operation", "Operations", createOperation);
    }

    ExecutionStrategy strategy;
    ExecutionStrategy::RunOptions options;
    std::vector<std::unique_ptr<DatabaseOperation>> operations;
};

void actor::RunCommand::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            config->strategy.run(
                [&](metrics::OperationContext& ctx) {
                    for (auto&& op : config->operations) {
                        op->run();
                    }
                },
                config->options);
        }
    }
}

actor::RunCommand::RunCommand(ActorContext& context)
    : Actor(context),
      _client{std::move(context.client())},
      _loop{context, context, _client, RunCommand::id()} {}

namespace {
auto registerRunCommand = Cast::registerDefault<actor::RunCommand>();

auto adminCommandProducer =
    std::make_shared<DefaultActorProducer<actor::RunCommand>>("AdminCommand");
auto registerAdminCommand = Cast::registerCustom(adminCommandProducer);
}  // namespace

}  // namespace genny
