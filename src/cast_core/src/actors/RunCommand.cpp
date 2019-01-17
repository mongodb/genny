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

#include <chrono>
#include <thread>

#include <boost/throw_exception.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>

#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <cast_core/actors/RunCommand.hpp>

#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/v1/RateLimiter.hpp>
#include <gennylib/value_generators.hpp>

namespace {

/**
 * @private
 * @param command passed to run_command()
 * @return if the command requires waiting for a stepdown to complete before continuing.
 */
bool requiresWaitForStepdown(bsoncxx::document::view& command) {
    return command.find("replSetStepDown") != command.end();
}

/**
 * @private
 * @param exception exception received from mongocxx::database::run_command()
 * @return if the exception is a result of a connection error or timeout
 */
bool isTimeout(mongocxx::operation_exception& exception) {
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
    auto request = builder << "collStats" << "__genny-arbitrary-collection" << bsoncxx::builder::stream::finalize;
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
void runThenAwaitStepdown(mongocxx::database &database, bsoncxx::document::view &command) {
    try {
        database.run_command(command);
        throw std::logic_error("Stepdown should throw operation exception");
    } catch (mongocxx::operation_exception& exception) {
        if (!isTimeout(exception)) {
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

}  // namespace


namespace genny {

/** @private */
class actor::RunCommand::Operation {
public:
    struct Fixture {
        PhaseContext& phaseContext;
        ActorContext& actorContext;

        ActorId id;
        std::string databaseName;
        mongocxx::database database;
    };

    using OpConfig = config::RunCommandConfig::Operation;

public:
    Operation(Fixture fixture,
              std::unique_ptr<value_generators::DocumentGenerator> docTemplate,
              OpConfig opts)
        : _databaseName{fixture.databaseName},
          _database{fixture.database},
          _doc{std::move(docTemplate)},
          _options{std::move(opts)},
          _rateLimiter{std::make_unique<v1::RateLimiterSimple>(_options.rateLimit)} {
        // Only record metrics if we have a name for the operation.
        if (!_options.metricsName.empty()) {
            _timer = std::make_optional<metrics::Timer>(
                fixture.actorContext.timer(_options.metricsName, fixture.id));
        }
    }

    void run() {
        _rateLimiter->run([&] { _run(); });
    }

private:
    void _run() {
        bsoncxx::builder::stream::document document{};
        auto view = _doc->view(document);
        if (!_options.isQuiet) {
            BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view)
                                    << " on database: " << _databaseName;
        }

        // If we have a timer, then we have a watch
        auto maybeWatch = _timer ? std::make_optional<metrics::RaiiStopwatch>(_timer->raii())
                                 : std::optional<metrics::RaiiStopwatch>(std::nullopt);

        try {
            // a bit of a hack
            if (requiresWaitForStepdown(view)) {
                runThenAwaitStepdown(_database, view);
            } else {
                _database.run_command(view);
            }
        } catch (mongocxx::operation_exception& e) {
            BOOST_THROW_EXCEPTION(MongoException(e, view));
        }
    }

    std::string _databaseName;
    mongocxx::database _database;
    std::unique_ptr<value_generators::DocumentGenerator> _doc;
    OpConfig _options;

    std::unique_ptr<v1::RateLimiter> _rateLimiter;
    std::optional<metrics::Timer> _timer;
};

/** @private */
struct actor::RunCommand::PhaseState {
    PhaseState(PhaseContext& context,
               ActorContext& actorContext,
               genny::DefaultRandom& rng,
               mongocxx::pool::entry& client,
               ActorId id)
        : strategy{actorContext, id, "RunCommand"},
          options{ExecutionStrategy::getOptionsFrom(context, "ExecutionStrategy")} {
        auto actorType = context.get<std::string>("Type");
        auto database = context.get<std::string, false>("Database").value_or("admin");
        if (actorType == "AdminCommand" && database != "admin") {
            throw InvalidConfigurationException(
                "AdminCommands can only be run on the 'admin' database.");
        }

        auto fixture = Operation::Fixture{
            context,
            actorContext,
            id,
            database,
            (*client)[database],
        };

        auto addOperation = [&](YAML::Node node) {
            auto yamlCommand = node["OperationCommand"];
            auto doc = value_generators::makeDoc(yamlCommand, rng);

            auto options = node.as<Operation::OpConfig>(Operation::OpConfig{});
            operations.push_back(std::make_unique<Operation>(fixture, std::move(doc), options));
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
    ExecutionStrategy::RunOptions options;
    std::vector<std::unique_ptr<Operation>> operations;
};

void actor::RunCommand::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            config->strategy.run(
                [&]() {
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
      _rng{context.workload().createRNG()},
      _client{std::move(context.client())},
      _loop{context, context, _rng, _client, RunCommand::id()} {}

namespace {
auto registerRunCommand = Cast::registerDefault<actor::RunCommand>();

auto adminCommandProducer =
    std::make_shared<DefaultActorProducer<actor::RunCommand>>("AdminCommand");
auto registerAdminCommand = Cast::registerCustom(adminCommandProducer);
}  // namespace

}  // namespace genny
