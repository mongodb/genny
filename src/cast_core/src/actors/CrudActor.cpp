#include <cast_core/actors/CrudActor.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>

#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/value_generators.hpp>

namespace {
using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;

using OpCallback = std::function<void(CrudActor::Operation&)>;

enum class StageType {
    Bucket,
};

std::unordered_map<std::string, StageType> stringToStageType = {{"bucket", StageType::Bucket}};

mongocxx::pipeline& addStage(mongocxx::pipeline& p, const YAML::Node& stage) {
    auto stageCommand = stage["StageCommand"];
    auto stageName = stage["StageName"].as<std::string>();
    auto stageType = stringToStageType.find(stageName);
    if (stageType == stringToStageType.end()) {
        throw genny::InvalidConfigurationException("Stage '" + stageName +
                                                   "' is not supported in Crud Actor.");
    }

    switch (stageType->second) {
        case StageType::Bucket:
            break;
    }
}

OpCallback aggregate = [](CrudActor::Operation& op) {
    mongocxx::pipeline p{};
    auto stages = op.opCommand["Stages"];
    if (!stages.IsSequence()) {
        throw genny::InvalidConfigurationException(
            "'Aggregate' requires a 'Stages' node of sequence type.");
    }
    for (auto&& stage : stages) {
    }
    auto onSession = op.opCommand["Session"];
};

std::unordered_map<std::string, OpCallback&> ops = {{"aggregate", aggregate}};
}  // namespace

namespace genny::actor {
CrudActor::Operation::Operation(CrudActor::Operation::Fixture fixture,
                                const YAML::Node& opCommand,
                                Options opts)
    : _database{fixture.database}, _options{opts}, callback{fixture.op}, opCommand{opCommand} {}

void CrudActor::Operation::run(const mongocxx::client_session& session) {
    callback(*this);
}

struct CrudActor::PhaseConfig {
    mongocxx::collection collection;
    std::unique_ptr<value_generators::DocumentGenerator> docGen;
    ExecutionStrategy::RunOptions options;
    std::vector<std::unique_ptr<Operation>> operations;

    PhaseConfig(PhaseContext& phaseContext, genny::DefaultRandom& rng, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          docGen{value_generators::makeDoc(phaseContext.get("Document"), rng)},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {

        auto addOperation = [&](YAML::Node node) {
            auto yamlCommand = node["OperationCommand"];
            auto opName = node["OperationName"].as<std::string>();
            auto doc = value_generators::makeDoc(yamlCommand, rng);

            auto options = node.as<Operation::Options>(Operation::Options{});

            auto op = ops.find(opName);

            if (op == ops.end()) {
                throw InvalidConfigurationException("Operation '" + opName +
                                                    "' not supported in Crud Actor.");
            }

            auto fixture = Operation::Fixture{
                phaseContext,
                db,
                op->second,
            };
            operations.push_back(std::make_unique<Operation>(fixture, yamlCommand, options));
        };

        auto operationList = phaseContext.get<YAML::Node, false>("Operations");
        auto operationUnit = phaseContext.get<YAML::Node, false>("Operation");
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
};

void CrudActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            try {
                auto session = _client->start_session();
                _strategy.run(
                    [&]() {
                        for (auto&& op : config->operations) {
                            op->run(session);
                        }
                    },
                    config->options);
            } catch (mongocxx::operation_exception& e) {
                // throw exception here.
            }
        }

        _strategy.recordMetrics();
    }
}

CrudActor::CrudActor(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _strategy{context, CrudActor::id(), "crud"},
      _client{std::move(context.client())},
      _loop{context, _rng, (*_client)[context.get<std::string>("Database")]} {}

namespace {


auto registerCrudActor = Cast::registerDefault<CrudActor>();
}  // namespace
}  // namespace genny::actor
