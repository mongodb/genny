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

namespace genny::actor {

namespace {

enum class StageType {
    Bucket,
    Count,
};

std::unordered_map<std::string, StageType> stringToStageType = {{"bucket", StageType::Bucket},
                                                                {"count", StageType::Count}};
};  // namespace

struct BaseOperation {
    virtual void run(const mongocxx::client_session& session) = 0;
    virtual ~BaseOperation() {}
};

using OpCallback = std::function<std::unique_ptr<BaseOperation>(
    YAML::Node, bool, mongocxx::collection, genny::DefaultRandom&)>;

struct AggregateOperation : public BaseOperation {

    struct Stage {
        std::string command;
        std::unique_ptr<DocGenerator> docTemplate;
    };

    AggregateOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng)
        : onSession{onSession}, collection{collection} {
        // BOOST_LOG_TRIVIAL(info) << "json: " << bsoncxx::to_json(opNode);
        auto yamlStages = opNode["Stages"];
        if (!yamlStages.IsSequence()) {
            throw InvalidConfigurationException(
                "'Aggregate' requires a 'Stages' node of sequence type.");
        }
        for (auto&& stage : yamlStages) {
            auto commandDoc = stage["Document"];
            auto stageCommand = stage["StageCommand"].as<std::string>();
            auto stageType = stringToStageType.find(stageCommand);
            if (stageType == stringToStageType.end()) {
                throw InvalidConfigurationException("Stage '" + stageCommand +
                                                    "' is not supported in Crud Actor.");
            }
            auto doc = value_generators::makeDoc(commandDoc, rng);
            auto stageObj = std::make_unique<Stage>(Stage{stageCommand, std::move(doc)});
            stages.push_back(std::move(stageObj));
        }
    }

    // options{config.get<mongocxx::options::aggregate>("Options")} {}

    virtual void run(const mongocxx::client_session& session) override {
        mongocxx::pipeline p{};
        for (auto&& stage : stages) {
            auto stageCommand = stage->command;
            auto stageType = stringToStageType.find(stageCommand);
            bsoncxx::builder::stream::document document{};
            auto view = stage->docTemplate->view(document);
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
        auto cursor = (onSession) ? collection.aggregate(p) : collection.aggregate(p);
        // TODO: exhaust cursor
    }

    mongocxx::collection collection;
    const bool onSession;
    std::vector<std::unique_ptr<Stage>> stages;
    // mongocxx::options::aggregate options;
};

struct BulkWriteOperation : public BaseOperation {

    struct WriteOperation {
        std::string opName;
        std::unique_ptr<DocGenerator> docTemplate;
    };

    std::vector<std::unique_ptr<WriteOperation>> writeOps;
    bool onSession;
    mongocxx::collection collection;

    BulkWriteOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng)
        : onSession{onSession}, collection{collection} {
        auto writeOpsYaml = opNode["WriteOperations"];
        if (!writeOpsYaml.IsSequence()) {
            throw InvalidConfigurationException(
                "'Bulk_Write' requires a 'WriteOperations' node of sequence type.");
        }
        for (auto&& writeOp : writeOpsYaml) {
            auto commandDoc = writeOp["Document"];
            auto writeCommand = writeOp["WriteCommand"].as<std::string>();
            auto doc = value_generators::makeDoc(commandDoc, rng);
            auto writeOpObj =
                std::make_unique<WriteOperation>(WriteOperation{writeCommand, std::move(doc)});
            writeOps.push_back(std::move(writeOpObj));
        }
    }

    void run(const mongocxx::client_session& session) {
        auto bulk = collection.create_bulk_write();
        for (auto&& writeOp : writeOps) {
            auto writeCommand = writeOp->opName;
            if (writeCommand == "insertOne") {
                bsoncxx::builder::stream::document document{};
                auto view = writeOp->docTemplate->view(document);
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
    }
};

OpCallback getAggregate = [](YAML::Node opNode,
                             bool onSession,
                             mongocxx::collection collection,
                             genny::DefaultRandom& rng) -> std::unique_ptr<BaseOperation> {
    return std::make_unique<AggregateOperation>(opNode, onSession, collection, rng);
};

OpCallback getBulkWrite = [](YAML::Node opNode,
                             bool onSession,
                             mongocxx::collection collection,
                             genny::DefaultRandom& rng) -> std::unique_ptr<BaseOperation> {
    return std::make_unique<BulkWriteOperation>(opNode, onSession, collection, rng);
};

std::unordered_map<std::string, OpCallback&> opConstructors = {
    {"aggregate", getAggregate},
    {"bulk_write", getBulkWrite},
};

/*
namespace YAML {
template <>
struct convert<mongocxx::options::aggregate> {
    // see conventions.hpp
};
}  // namespace YAML*/

struct CrudActor::PhaseConfig {
    mongocxx::collection collection;
    ExecutionStrategy::RunOptions options;
    std::vector<std::unique_ptr<BaseOperation>> operations;

    PhaseConfig(PhaseContext& phaseContext, genny::DefaultRandom& rng, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {

        auto addOperation = [&](YAML::Node node) {
            auto yamlCommand = node["OperationCommand"];
            auto opName = node["OperationName"].as<std::string>();
            auto onSession = yamlCommand["Session"] && yamlCommand["Session"].as<bool>();
            auto op = opConstructors.find(opName);
            if (op == opConstructors.end()) {
                throw InvalidConfigurationException("Operation '" + opName +
                                                    "' not supported in Crud Actor.");
            }
            auto createOperation = op->second;
            std::vector<std::pair<std::string, std::unique_ptr<DocGenerator>>> docs;
            operations.emplace_back(createOperation(yamlCommand, onSession, collection, rng));
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

namespace {
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}
}  // namespace genny::actor
