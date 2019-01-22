#include <cast_core/actors/CrudActor.hpp>
#include <chrono>
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
#include <gennylib/conventions.hpp>
#include <gennylib/value_generators.hpp>

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;
using DocGenerator = genny::value_generators::DocumentGenerator;

namespace YAML {

template <>
struct convert<mongocxx::read_preference> {
    using ReadPreference = mongocxx::read_preference;
    using ReadMode = mongocxx::read_preference::read_mode;
    static Node encode(const ReadPreference& rhs) {
        Node node;
        auto mode = rhs.mode();
        if (mode == ReadMode::k_primary) {
            node["ReadMode"] = "primary";
        } else if (mode == ReadMode::k_primary_preferred) {
            node["ReadMode"] = "primaryPreferred";
        } else if (mode == ReadMode::k_secondary) {
            node["ReadMode"] = "secondary";
        } else if (mode == ReadMode::k_secondary_preferred) {
            node["ReadMode"] = "secondaryPreferred";
        } else if (mode == ReadMode::k_nearest) {
            node["ReadMode"] = "nearest";
        }
        auto maxStaleness = rhs.max_staleness();
        if (maxStaleness) {
            node["MaxStaleness"] = genny::TimeSpec(*maxStaleness);
        }
        return node;
    }

    static bool decode(const Node& node, ReadPreference& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (!node["ReadMode"]) {
            // readPreference must have a read mode specified.
            return false;
        }
        auto readMode = node["ReadMode"].as<std::string>();
        rhs = mongocxx::read_preference{};
        if (readMode == "primary") {
            rhs.mode(ReadMode::k_primary);
        } else if (readMode == "primaryPreferred") {
            rhs.mode(ReadMode::k_primary_preferred);
        } else if (readMode == "secondary") {
            rhs.mode(ReadMode::k_secondary);
        } else if (readMode == "secondaryPreferred") {
            rhs.mode(ReadMode::k_secondary_preferred);
        } else if (readMode == "nearest") {
            rhs.mode(ReadMode::k_nearest);
        } else {
            return false;
        }
        if (node["MaxStaleness"]) {
            auto maxStaleness = node["MaxStaleness"].as<genny::TimeSpec>();
            rhs.max_staleness(std::chrono::seconds(maxStaleness));
        }
        return true;
    }
};

template <>
struct convert<mongocxx::write_concern> {
    using WriteConcern = mongocxx::write_concern;
    static Node encode(const WriteConcern& rhs) {
        Node node;
        auto timeout = rhs.timeout();
        auto timeoutTimeSpec = genny::TimeSpec{timeout};
        node["TimeOutMillis"] = timeoutTimeSpec;

        return node;
    }

    static bool decode(const Node& node, WriteConcern& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (!node["Level"]) {
            // writeConcern must specify the write concern level.
            return false;
        }
        auto level = node["Level"].as<std::string>();
        bool isNum = level.find_first_not_of("0123456789") == std::string::npos;
        if (isNum) {
            auto levelNum = node["Level"].as<int>();
            rhs.nodes(levelNum);
        } else if (level == "majority") {
            rhs.majority(std::chrono::milliseconds{0});
        } else {
            // writeConcern level must be of valid integer or 'majority'.
            return false;
        }
        BOOST_LOG_TRIVIAL(info) << "done levels";
        if (node["TimeOutMillis"]) {
            auto timeout = node["TimeOutMillis"].as<genny::TimeSpec>();
            rhs.timeout(std::chrono::milliseconds(timeout));
        }
        BOOST_LOG_TRIVIAL(info) << "done timeout";
        if (node["Journal"]) {
            auto journal = node["Journal"].as<bool>();
            rhs.journal(journal);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::aggregate> {
    using AggregateOptions = mongocxx::options::aggregate;
    static Node encode(const AggregateOptions& rhs) {
        Node node;
        auto allowDiskUse = rhs.allow_disk_use();
        if (allowDiskUse) {
            node["allowDiskUse"] = *allowDiskUse;
        }
        auto batchSize = rhs.batch_size();
        if (batchSize) {
            node["batchSize"] = *batchSize;
        }
        return node;
    }

    static bool decode(const Node& node, AggregateOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        rhs = mongocxx::options::aggregate{};
        if (node["allowDiskUse"]) {
            auto allowDiskUse = node["allowDiskUse"].as<bool>();
            rhs.allow_disk_use(allowDiskUse);
        }
        if (node["batchSize"]) {
            auto batchSize = node["batchSize"].as<int>();
            rhs.batch_size(batchSize);
        }
        if (node["maxTime"]) {
            auto maxTime = node["maxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds(maxTime));
        }
        if (node["readPreference"]) {
            auto readPreference = node["readPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPreference);
        }
        if (node["bypassDocumentValidation"]) {
            auto bypassValidation = node["bypassDocumentValidation"].as<bool>();
            rhs.bypass_document_validation(bypassValidation);
        }
        if (node["hint"]) {
            auto h = node["hint"].as<std::string>();
            auto hint = mongocxx::hint(h);
            rhs.hint(hint);
        }
        if (node["writeConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::bulk_write> {
    using BulkWriteOptions = mongocxx::options::bulk_write;
    static Node encode(const BulkWriteOptions& rhs) {
        Node node;
        auto bypassDocValidation = rhs.bypass_document_validation();
        if (bypassDocValidation) {
            node["BypassDocumentValidation"] = *bypassDocValidation;
        }
        auto isOrdered = rhs.ordered();
        node["Ordered"] = isOrdered;
        return node;
    }

    static bool decode(const Node& node, BulkWriteOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["BypassDocumentValidation"]) {
            auto bypassDocValidation = node["BypassDocumentValidation"].as<bool>();
            rhs.bypass_document_validation(bypassDocValidation);
        }
        if (node["Ordered"]) {
            auto isOrdered = node["ordered"].as<bool>();
            rhs.ordered(isOrdered);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return true;
    }
};
}  // namespace YAML

namespace genny::actor {

namespace {

enum class StageType {
    Bucket,
    Count,
};

std::unordered_map<std::string, StageType> stringToStageType = {{"bucket", StageType::Bucket},
                                                                {"count", StageType::Count}};

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
        // TODO: exhaust cursor and report metrics
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
    mongocxx::options::bulk_write options;

    BulkWriteOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng)
        : onSession{onSession}, collection{collection} {
        auto writeOpsYaml = opNode["WriteOperations"];
        if (!writeOpsYaml.IsSequence()) {
            throw InvalidConfigurationException(
                "'bulkWrite' requires a 'WriteOperations' node of sequence type.");
        }
        for (auto&& writeOp : writeOpsYaml) {
            auto commandDoc = writeOp["Document"];
            auto writeCommand = writeOp["WriteCommand"].as<std::string>();
            auto doc = value_generators::makeDoc(commandDoc, rng);
            auto writeOpObj =
                std::make_unique<WriteOperation>(WriteOperation{writeCommand, std::move(doc)});
            writeOps.push_back(std::move(writeOpObj));
        }
        auto yamlOptions = opNode["Options"];
        if (yamlOptions) {
            options = yamlOptions.as<mongocxx::options::bulk_write>();
        }
    }

    virtual void run(const mongocxx::client_session& session) override {
        auto bulk = (onSession) ? collection.create_bulk_write(session, options)
                                : collection.create_bulk_write(options);
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
            // throw exception
        }
    }
};

struct InsertOperation : public BaseOperation {};

OpCallback createAggregate = [](YAML::Node opNode,
                                bool onSession,
                                mongocxx::collection collection,
                                genny::DefaultRandom& rng) -> std::unique_ptr<BaseOperation> {
    return std::make_unique<AggregateOperation>(opNode, onSession, collection, rng);
};

OpCallback createBulkWrite = [](YAML::Node opNode,
                                bool onSession,
                                mongocxx::collection collection,
                                genny::DefaultRandom& rng) -> std::unique_ptr<BaseOperation> {
    return std::make_unique<BulkWriteOperation>(opNode, onSession, collection, rng);
};

// Maps the yaml 'OperationName' string to the appropriate constructor of 'BaseOperation' type.
std::unordered_map<std::string, OpCallback&> opConstructors = {
    {"aggregate", createAggregate},
    {"bulkWrite", createBulkWrite},
};

};  // namespace

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

            // Grab the appropriate
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
