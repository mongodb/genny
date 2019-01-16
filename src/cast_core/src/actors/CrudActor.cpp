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

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;
using DocGenerator = genny::value_generators::DocumentGenerator;
using OpCallback = std::function<void(CrudActor::Operation&, const mongocxx::client_session&)>;

namespace genny::actor {

class CrudActor::Operation {

public:
    struct Fixture {
        PhaseContext& phaseContext;
        mongocxx::collection collection;
        bool onSession;
        YAML::Node opCommand;
    };

    using Options = config::RunCommandConfig::Operation;
    mongocxx::collection collection;
    const OpCallback& runOp;
    bool onSession;
    Fixture fixture;
    std::vector<std::pair<std::string, std::unique_ptr<DocGenerator>>> commandDocs;
    YAML::Node opCommand;

public:
    Operation(Fixture fixture,
              OpCallback& op,
              std::vector<std::pair<std::string, std::unique_ptr<DocGenerator>>> docTemplates,
              Options opts)
        : fixture{std::move(fixture)},
          collection{fixture.collection},
          _options{opts},
          runOp{op},
          onSession{fixture.onSession},
          opCommand{fixture.opCommand},
          commandDocs{std::move(docTemplates)} {}

    void run(const mongocxx::client_session& session) {
        runOp(*this, session);
    }

private:
    mongocxx::database _database;
    std::unique_ptr<DocGenerator> _doc;
    Options _options;
    std::optional<metrics::Timer> _timer;
};

namespace {
enum class StageType {
    Bucket,
    Count,
};

std::unordered_map<std::string, StageType> stringToStageType = {{"bucket", StageType::Bucket},
                                                                {"count", StageType::Count}};

OpCallback aggregate = [](CrudActor::Operation& op, const mongocxx::client_session& session) {
    mongocxx::pipeline p{};
    for (auto&& commandDocPair : op.commandDocs) {
        auto stageCommand = commandDocPair.first;
        auto stageType = stringToStageType.find(stageCommand);
        bsoncxx::builder::stream::document document{};
        auto view = commandDocPair.second->view(document);
        if (stageType == stringToStageType.end()) {
            throw InvalidConfigurationException("Aggregation stage '" + stageCommand +
                                                "' is not supported in Crud Actor.");
        }
        switch (stageType->second) {
            case StageType::Bucket:
                p.bucket(view);
                break;
            case StageType::Count:
                auto field = view["field"].get_utf8().value;
                auto fieldNamestring = std::string(field);
                p.count(fieldNamestring);
                break;
        }
    }
    auto cursor = (op.onSession) ? op.collection.aggregate(p) : op.collection.aggregate(p);
    // TODO: exhaust cursor
};

OpCallback bulkWrite = [](CrudActor::Operation& op, const mongocxx::client_session& session) {
    auto bulk = op.collection.create_bulk_write();
    for (auto&& commandDocPair : op.commandDocs) {
        auto writeCommand = commandDocPair.first;
        if (writeCommand == "insertOne") {
            bsoncxx::builder::stream::document document{};
            auto view = commandDocPair.second->view(document);
            mongocxx::model::insert_one insert_op{view};
            bulk.append(insert_op);
        } else if (writeCommand == "updateOne") {
            // append 'updateOne' model to bulk
        } else if (writeCommand == "updateMany") {
            // append 'updateMany' model to bulk
        } else if (writeCommand == "deleteOne") {
            // append 'deleteOne' model to bulk
        } else if (writeCommand == "deleteMany") {
            // append 'deleteMany' model to bulk
        } else if (writeCommand == "replaceOne") {
            // append 'replaceOne' model to bulk
        }
    }

    auto result = bulk.execute();
    if (!result) {
        throw InvalidConfigurationException("bulk write failed!");
    }
};

std::unordered_map<std::string, OpCallback&> ops = {{"aggregate", aggregate},
                                                    {"bulk_write", bulkWrite}};
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}  // namespace

struct CrudActor::PhaseConfig {
    mongocxx::collection collection;
    ExecutionStrategy::RunOptions options;
    std::vector<std::unique_ptr<Operation>> operations;

    PhaseConfig(PhaseContext& phaseContext, genny::DefaultRandom& rng, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {

        auto addOperation = [&](YAML::Node node) {
            auto yamlCommand = node["OperationCommand"];
            auto opName = node["OperationName"].as<std::string>();
            auto options = node.as<Operation::Options>(Operation::Options{});

            auto op = ops.find(opName);

            if (op == ops.end()) {
                throw InvalidConfigurationException("Operation '" + opName +
                                                    "' not supported in Crud Actor.");
            }

            std::vector<std::pair<std::string, std::unique_ptr<DocGenerator>>> docs;
            if (opName == "aggregate") {
                auto stages = yamlCommand["Stages"];
                if (!stages.IsSequence()) {
                    throw InvalidConfigurationException(
                        "'Aggregate' requires a 'Stages' node of sequence type.");
                }
                for (auto&& stage : stages) {
                    auto commandDoc = stage["Document"];
                    auto stageCommand = stage["StageCommand"].as<std::string>();
                    auto stageType = stringToStageType.find(stageCommand);
                    if (stageType == stringToStageType.end()) {
                        throw InvalidConfigurationException("Stage '" + stageCommand +
                                                            "' is not supported in Crud Actor.");
                    }
                    auto doc = value_generators::makeDoc(commandDoc, rng);
                    docs.push_back({stageCommand, std::move(doc)});
                }
            } else if (opName == "bulk_write") {
                auto writeOps = yamlCommand["WriteOperations"];
                if (!writeOps.IsSequence()) {
                    throw InvalidConfigurationException(
                        "'Bulk_Write' requires a 'WriteOperations' node of sequence type.");
                }
                for (auto&& writeOp : writeOps) {
                    auto commandDoc = writeOp["Document"];
                    auto writeCommand = writeOp["WriteCommand"].as<std::string>();
                    auto doc = value_generators::makeDoc(commandDoc, rng);
                    docs.push_back({writeCommand, std::move(doc)});
                }
            } else {
                // For simpler cases like 'insert_one' where docs is a vector of pairs of size one.
            }
            auto onSession = yamlCommand["Session"] && yamlCommand["Session"].as<bool>();
            auto fixture = Operation::Fixture{phaseContext, collection, onSession, yamlCommand};
            operations.push_back(std::make_unique<Operation>(
                std::move(fixture), op->second, std::move(docs), options));
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
            auto session = _client->start_session();
            _strategy.run(
                [&]() {
                    for (auto&& op : config->operations) {
                        op->run(session);
                    }
                },
                config->options);
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
}  // namespace genny::actor
