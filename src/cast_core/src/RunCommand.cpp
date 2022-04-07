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

#include <boost/log/trivial.hpp>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/Node.hpp>

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
    explicit RunCommandOperationConfig(const genny::Node& node)
        : metricsName{node["OperationMetricsName"].maybe<std::string>().value_or("")},
          isQuiet{node["OperationIsQuiet"].maybe<bool>().value_or(false)},
          awaitStepdown{node["OperationAwaitStepdown"].maybe<bool>().value_or(false)} {
        if (auto opName = node["OperationName"].maybe<std::string>();
            opName != "RunCommand" && opName != "AdminCommand") {
            BOOST_THROW_EXCEPTION(genny::InvalidConfigurationException(
                "Operation name '" + *opName +
                "' not recognized. Needs either 'RunCommand' or 'AdminCommand'."));
        }
    }
    explicit RunCommandOperationConfig() {}

    const std::string metricsName = "";
    const bool isQuiet = false;
    const bool awaitStepdown = false;
};

}  // namespace

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
          // Record metrics for the operation or the phase depending on
          // whether metricsName is set for the operation.
          //
          // Note: actorContext.operation() must be called to use
          // OperationMetricsName. phaseContext.operation() will try to
          // override the name with the metrics name for the phase.
          _operation{!_options.metricsName.empty()
                         ? std::make_optional<metrics::Operation>(
                               actorContext.operation(_options.metricsName, id))
                         : phaseContext.operation("DatabaseOperation", id)} {}

    static std::unique_ptr<DatabaseOperation> create(const Node& node,
                                                     PhaseContext& context,
                                                     ActorContext& actorContext,
                                                     ActorId id,
                                                     mongocxx::pool::entry& client,
                                                     const std::string& database) {
        auto& yamlCommand = node["OperationCommand"];
        auto commandExpr = yamlCommand.to<DocumentGenerator>(context, id);

        auto options =
            node.maybe<DatabaseOperation::OpConfig>().value_or(DatabaseOperation::OpConfig{});
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
            try {
                if (_options.awaitStepdown) {
                    runThenAwaitStepdown(_database, view);
                } else {
                    _database.run_command(view);
                }

                if (maybeWatch) {
                    maybeWatch->success();
                }
            } catch (mongocxx::operation_exception& e) {
                BOOST_THROW_EXCEPTION(MongoException(e, view));
            }
        } catch (...) {
            if (maybeWatch) {
                maybeWatch->failure();
            }

            throw;
        }
    }

    bool isQuiet() const {
        return _options.isQuiet;
    }

private:
    std::string _databaseName;
    mongocxx::database _database;
    DocumentGenerator _commandExpr;
    OpConfig _options;

    std::optional<metrics::Operation> _operation;
    bool _awaitStepdown;
};

class InstanceTypeFilter {
public:
    struct Options {
        static constexpr std::string_view kStandalone = "standalone";
        static constexpr std::string_view kSharded = "sharded";
        static constexpr std::string_view kReplicaSet = "replica_set";

        static std::string getValidOptionsListAsString() {
            std::stringstream listString;
            listString << kStandalone << ", " << kSharded << ", " << kReplicaSet;
            return listString.str();
        }

        static bool isValid(std::string_view instanceType) {
            return instanceType == kStandalone || instanceType == kSharded 
                    || instanceType == kReplicaSet;
        }

        static bool isValidList(const std::vector<std::string>& options) {
            for(auto &option : options) {
                if(!isValid(option)) {
                    return false;
                }
            }
            return true;
        }
    };

    InstanceTypeFilter(PhaseContext &context, mongocxx::pool::entry& client) {
        try {
            _onlyRunInInstancesContext = context.getPlural<std::string>(
                "OnlyRunInInstance", "OnlyRunInInstances", [&](const Node& node) {
                    return node.to<std::string>();
            });
        } catch(...) {
            // If the key exists there is a configuration error
            if(context["OnlyRunInInstance"] || context["OnlyRunInInstance"]) {
                std::rethrow_exception(std::current_exception());
            }
        }

        _typeFoundInConfig = false;
        if(_onlyRunInInstancesContext.size()) {
            if(!Options::isValidList(_onlyRunInInstancesContext)) {
                std::stringstream errorMsg;
                errorMsg << "OnlyRunInInstance or OnlyRunInInstances valid values are: "
                    << Options::getValidOptionsListAsString();
                throw InvalidConfigurationException(errorMsg.str());
            }

            auto adminDB = (*client)["admin"];
            _type = getInstanceType(adminDB);
            for(const auto &configInstanceType : _onlyRunInInstancesContext) {
                if(_type == configInstanceType) {
                    _typeFoundInConfig = true;
                }
            }
        }
    }

    bool shouldRun() {
        return _onlyRunInInstancesContext.size() == 0 || _typeFoundInConfig;
    }

    static std::string_view getInstanceType(mongocxx::database &db) {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;

        auto helloResult = db.run_command(make_document(kvp("hello", 1)));
        if(auto msg = helloResult.view()["msg"]) {
            return Options::kSharded;
        }
        if(auto msg = helloResult.view()["hosts"]) {
            return Options::kReplicaSet;
        }
        return Options::kStandalone;
    }

private:
    std::vector<std::string> _onlyRunInInstancesContext;
    bool _typeFoundInConfig;
    std::string_view _type;
};

/** @private */
struct actor::RunCommand::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                ActorContext& actorContext,
                mongocxx::pool::entry& client,
                ActorId id)
        : throwOnFailure{context["ThrowOnFailure"].maybe<bool>().value_or(true)} {
        auto actorType = actorContext["Type"].to<std::string>();
        auto database = context["Database"].maybe<std::string>().value_or("admin");
        if (actorType == "AdminCommand" && database != "admin") {
            throw InvalidConfigurationException(
                "AdminCommands can only be run on the 'admin' database.");
        }

        InstanceTypeFilter onlyRunInInstancesFilter(context, client);
        // Run when there are no OnlyRunInInstances option or when one of the options matches
        if(onlyRunInInstancesFilter.shouldRun()) {
            auto createOperation = [&](const Node& node) {
                return DatabaseOperation::create(node, context, actorContext, id, client, database);
            };

            operations = context.getPlural<std::unique_ptr<DatabaseOperation>>(
                "Operation", "Operations", createOperation);
        }
    }

    bool throwOnFailure;
    std::vector<std::unique_ptr<DatabaseOperation>> operations;
};

void actor::RunCommand::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            for (auto&& op : config->operations) {
                try {
                    op->run();
                } catch (const boost::exception& ex) {
                    if (config->throwOnFailure) {
                        throw;
                    }

                    if (!op->isQuiet()) {
                        BOOST_LOG_TRIVIAL(info)
                            << "Caught error: " << boost::diagnostic_information(ex);
                    }
                }
            }
        }
    }
}

actor::RunCommand::RunCommand(ActorContext& context)
    : Actor(context),
      _client{std::move(
          context.client(context.get("ClientName").maybe<std::string>().value_or("Default")))},
      _loop{context, context, _client, RunCommand::id()} {}

namespace {
auto registerRunCommand = Cast::registerDefault<actor::RunCommand>();

auto adminCommandProducer =
    std::make_shared<DefaultActorProducer<actor::RunCommand>>("AdminCommand");
auto registerAdminCommand = Cast::registerCustom(adminCommandProducer);
}  // namespace

}  // namespace genny
