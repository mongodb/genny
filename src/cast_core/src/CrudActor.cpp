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

#include <cast_core/actors/CrudActor.hpp>

#include <chrono>
#include <memory>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;

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
            rhs.max_staleness(std::chrono::seconds{maxStaleness});
        }
        return true;
    }
};

template <>
struct convert<mongocxx::read_concern> {
    using ReadConcern = mongocxx::read_concern;
    using ReadConcernLevel = mongocxx::read_concern::level;
    static Node encode(const ReadConcern& rhs) {
        Node node;
        auto level = std::string(rhs.acknowledge_string());
        if (!level.empty()) {
            node["Level"] = level;
        }
        return node;
    }

    static bool isValidReadConcernString(std::string_view rcString) {
        return (rcString == "local" || rcString == "majority" || rcString == "linearizable" ||
                rcString == "snapshot" || rcString == "available");
    }

    static bool decode(const Node& node, ReadConcern& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (!node["Level"]) {
            // readConcern must have a read concern level specified.
            return false;
        }
        auto level = node["Level"].as<std::string>();
        if (isValidReadConcernString(level)) {
            rhs.acknowledge_string(level);
            return true;
        } else {
            return false;
        }
    }
};

template <>
struct convert<mongocxx::write_concern> {
    using WriteConcern = mongocxx::write_concern;
    static Node encode(const WriteConcern& rhs) {
        Node node;
        node["Timeout"] = genny::TimeSpec{rhs.timeout()};

        auto journal = rhs.journal();
        node["Journal"] = journal;

        auto ackLevel = rhs.acknowledge_level();
        if (ackLevel == WriteConcern::level::k_majority) {
            node["Level"] = "majority";
        } else if (ackLevel == WriteConcern::level::k_acknowledged) {
            auto numNodes = rhs.nodes();
            if (numNodes) {
                node["Level"] = *numNodes;
            }
        }
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
        try {
            auto level = node["Level"].as<int>();
            rhs.nodes(level);
        } catch (const BadConversion& e) {
            auto level = node["Level"].as<std::string>();
            if (level == "majority") {
                rhs.majority(std::chrono::milliseconds{0});
            } else {
                // writeConcern level must be of valid integer or 'majority'.
                return false;
            }
        }
        if (node["Timeout"]) {
            auto timeout = node["Timeout"].as<genny::TimeSpec>();
            rhs.timeout(std::chrono::milliseconds{timeout});
        }
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
        if (node["AllowDiskUse"]) {
            auto allowDiskUse = node["AllowDiskUse"].as<bool>();
            rhs.allow_disk_use(allowDiskUse);
        }
        if (node["BatchSize"]) {
            auto batchSize = node["BatchSize"].as<int>();
            rhs.batch_size(batchSize);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds(maxTime));
        }
        if (node["ReadPreference"]) {
            auto readPreference = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPreference);
        }
        if (node["BypassDocumentValidation"]) {
            auto bypassValidation = node["BypassDocumentValidation"].as<bool>();
            rhs.bypass_document_validation(bypassValidation);
        }
        if (node["Hint"]) {
            auto h = node["Hint"].as<std::string>();
            auto hint = mongocxx::hint(h);
            rhs.hint(hint);
        }
        if (node["WriteConcern"]) {
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
            auto isOrdered = node["Ordered"].as<bool>();
            rhs.ordered(isOrdered);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::count> {
    using CountOptions = mongocxx::options::count;
    static Node encode(const CountOptions& rhs) {
        Node node;
        auto h = rhs.hint();
        if (h) {
            auto hint = h->to_value().get_utf8().value;
            node["Hint"] = std::string(hint);
        }
        auto limit = rhs.limit();
        if (limit) {
            node["Limit"] = *limit;
        }
        auto maxTime = rhs.max_time();
        if (maxTime) {
            node["MaxTime"] = genny::TimeSpec(*maxTime);
        }
        return node;
    }

    static bool decode(const Node& node, CountOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["Hint"]) {
            auto h = node["Hint"].as<std::string>();
            auto hint = mongocxx::hint(h);
            rhs.hint(hint);
        }
        if (node["Limit"]) {
            auto limit = node["Limit"].as<int>();
            rhs.limit(limit);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::transaction> {
    using TransactionOptions = mongocxx::options::transaction;
    static Node encode(const TransactionOptions& rhs) {
        Node node;
        return node;
    }

    static bool decode(const Node& node, TransactionOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        if (node["ReadConcern"]) {
            auto rc = node["ReadConcern"].as<mongocxx::read_concern>();
            rhs.read_concern(rc);
        }
        if (node["ReadPreference"]) {
            auto rp = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(rp);
        }
        return true;
    }
};
}  // namespace YAML

namespace {

using namespace genny;

struct BaseOperation {
    virtual void run(mongocxx::client_session& session) = 0;
    virtual ~BaseOperation() = default;
};

using OpCallback = std::function<std::unique_ptr<BaseOperation>(
    YAML::Node, bool, mongocxx::collection, genny::DefaultRandom&, metrics::Operation)>;

struct WriteOperation : public BaseOperation {
    virtual mongocxx::model::write getModel() = 0;
};

using WriteOpCallback = std::function<std::unique_ptr<WriteOperation>(
    YAML::Node, bool, mongocxx::collection, genny::DefaultRandom&, metrics::Operation)>;

namespace {


auto createGenerator(YAML::Node source,
                     const std::string& opType,
                     const std::string& key,
                     DefaultRandom& rng) {
    auto doc = source[key];
    if (!doc) {
        std::stringstream msg;
        msg << "'" << opType << "' expects a '" << key << "' field.";
        throw InvalidConfigurationException(msg.str());
    }
    return Generators::document(doc, rng);
}

}  // namespace

struct InsertOneOperation : public WriteOperation {
    InsertOneOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng,
                       metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _docExpr{createGenerator(opNode, "insertOne", "Document", rng)} {

        // TODO: parse insert options.
    }

    mongocxx::model::write getModel() override {
        auto document = _docExpr();
        return mongocxx::model::insert_one{std::move(document)};
    }

    void run(mongocxx::client_session& session) override {
        auto document = _docExpr();
        auto ctx = _operation.start();
        (_onSession) ? _collection.insert_one(session, std::move(document), _options)
                     : _collection.insert_one(std::move(document), _options);
        ctx.success();
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    genny::DefaultRandom& _rng;
    DocumentGenerator _docExpr;
    metrics::Operation _operation;
    mongocxx::options::insert _options;
};


struct UpdateOneOperation : public WriteOperation {
    UpdateOneOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng,
                       metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "updateOne", "Filter", rng)},
          _updateExpr{createGenerator(opNode, "updateOne", "Update", rng)} {}
    // TODO: parse update options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        return mongocxx::model::update_one{std::move(filter), std::move(update)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        auto ctx = _operation.start();
        (_onSession)
            ? _collection.update_one(session, std::move(filter), std::move(update), _options)
            : _collection.update_one(std::move(filter), std::move(update), _options);
        ctx.success();
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    DocumentGenerator _updateExpr;
    metrics::Operation _operation;
    mongocxx::options::update _options;
};

struct UpdateManyOperation : public WriteOperation {
    UpdateManyOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        genny::DefaultRandom& rng,
                        metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "updateMany", "Filter", rng)},
          _updateExpr{createGenerator(opNode, "updateMany", "Update", rng)} {}

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        return mongocxx::model::update_many{std::move(filter), std::move(update)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        auto ctx = _operation.start();
        (_onSession)
            ? _collection.update_many(session, std::move(filter), std::move(update), _options)
            : _collection.update_many(std::move(filter), std::move(update), _options);
        ctx.success();
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    DocumentGenerator _updateExpr;
    metrics::Operation _operation;
    mongocxx::options::update _options;
};

struct DeleteOneOperation : public WriteOperation {
    DeleteOneOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng,
                       metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr(createGenerator(opNode, "deleteOne", "Filter", rng)) {}
    // TODO: parse delete options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        return mongocxx::model::delete_one{std::move(filter)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto ctx = _operation.start();
        (_onSession) ? _collection.delete_one(session, std::move(filter), _options)
                     : _collection.delete_one(std::move(filter), _options);
        ctx.success();
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
    mongocxx::options::delete_options _options;
};

struct DeleteManyOperation : public WriteOperation {
    DeleteManyOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        genny::DefaultRandom& rng,
                        metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "deleteMany", "Filter", rng)} {}
    // TODO: parse delete options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        return mongocxx::model::delete_many{std::move(filter)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto ctx = _operation.start();
        (_onSession) ? _collection.delete_many(session, std::move(filter), _options)
                     : _collection.delete_many(std::move(filter), _options);
        ctx.success();
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
    mongocxx::options::delete_options _options;
};

struct ReplaceOneOperation : public WriteOperation {
    ReplaceOneOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        genny::DefaultRandom& rng,
                        metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "replaceOne", "Filter", rng)},
          _replacementExpr{createGenerator(opNode, "replaceOne", "Replacement", rng)} {}

    // TODO: parse replace options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        auto replacement = _replacementExpr();
        return mongocxx::model::replace_one{std::move(filter), std::move(replacement)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto replacement = _replacementExpr();
        auto ctx = _operation.start();
        (_onSession)
            ? _collection.replace_one(session, std::move(filter), std::move(replacement), _options)
            : _collection.replace_one(std::move(filter), std::move(replacement), _options);
        ctx.success();
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    DocumentGenerator _replacementExpr;
    metrics::Operation _operation;
    mongocxx::options::replace _options;
};

template <class P, class C, class O>
C baseCallback = [](YAML::Node opNode,
                    bool onSession,
                    mongocxx::collection collection,
                    genny::DefaultRandom& rng,
                    metrics::Operation operation) -> std::unique_ptr<P> {
    return std::make_unique<O>(opNode, onSession, collection, rng, operation);
};

// Maps the WriteCommand name to the constructor of the designated Operation struct.
std::unordered_map<std::string, WriteOpCallback&> bulkWriteConstructors = {
    {"insertOne", baseCallback<WriteOperation, WriteOpCallback, InsertOneOperation>},
    {"updateOne", baseCallback<WriteOperation, WriteOpCallback, UpdateOneOperation>},
    {"deleteOne", baseCallback<WriteOperation, WriteOpCallback, DeleteOneOperation>},
    {"deleteMany", baseCallback<WriteOperation, WriteOpCallback, DeleteManyOperation>},
    {"replaceOne", baseCallback<WriteOperation, WriteOpCallback, ReplaceOneOperation>},
    {"updateMany", baseCallback<WriteOperation, WriteOpCallback, UpdateManyOperation>}};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: bulkWrite
 *      OperationCommand:
 *        WriteOperations:
 *        - WriteCommand: insertOne
 *          Document: { a: 1 }
 *        - WriteCommand: updateOne
 *          Filter: { a: 1 }
 *          Update: { $set: { a: 5 } }
 *        Options:
 *          Ordered: true
 *          WriteConcern:
 *            Level: majority
 *            Journal: true
 *        OnSession: false
 */
struct BulkWriteOperation : public BaseOperation {

    BulkWriteOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       genny::DefaultRandom& rng,
                       metrics::Operation operation)
        : _onSession{onSession}, _collection{collection}, _operation{operation} {
        auto writeOpsYaml = opNode["WriteOperations"];
        if (!writeOpsYaml.IsSequence()) {
            throw InvalidConfigurationException(
                "'bulkWrite' requires a 'WriteOperations' node of sequence type.");
        }
        for (auto&& writeOp : writeOpsYaml) {
            createOps(writeOp, rng);
        }
        if (opNode["Options"]) {
            _options = opNode["Options"].as<mongocxx::options::bulk_write>();
        }
    }

    void createOps(const YAML::Node& writeOp, genny::DefaultRandom& rng) {
        auto writeCommand = writeOp["WriteCommand"].as<std::string>();
        auto writeOpConstructor = bulkWriteConstructors.find(writeCommand);
        if (writeOpConstructor == bulkWriteConstructors.end()) {
            throw InvalidConfigurationException("WriteCommand '" + writeCommand +
                                                "' not supported in bulkWrite operations.");
        }
        auto createWriteOp = writeOpConstructor->second;
        _writeOps.push_back(createWriteOp(writeOp, _onSession, _collection, rng, _operation));
    }

    void run(mongocxx::client_session& session) override {
        auto bulk = (_onSession) ? _collection.create_bulk_write(session, _options)
                                 : _collection.create_bulk_write(_options);
        for (auto&& op : _writeOps) {
            auto writeModel = op->getModel();
            bulk.append(writeModel);
        }
        auto ctx = _operation.start();
        auto result = bulk.execute();
        ctx.success();
    }

private:
    std::vector<std::unique_ptr<WriteOperation>> _writeOps;
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::bulk_write _options;
    metrics::Operation _operation;
};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: count
 *      OperationCommand:
 *        Filter: { a : 1 }
 *        Options:
 *          Limit: 5
 *        OnSession: false
 */
struct CountDocumentsOperation : public BaseOperation {
    CountDocumentsOperation(YAML::Node opNode,
                            bool onSession,
                            mongocxx::collection collection,
                            genny::DefaultRandom& rng,
                            metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "Count", "Filter", rng)} {
        if (opNode["Options"]) {
            _options = opNode["Options"].as<mongocxx::options::count>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto ctx = _operation.start();
        (_onSession) ? _collection.count_documents(session, std::move(filter), _options)
                     : _collection.count_documents(std::move(filter), _options);
        ctx.success();
    }


private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::count _options;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
};

struct FindOperation : public BaseOperation {
    FindOperation(YAML::Node opNode,
                  bool onSession,
                  mongocxx::collection collection,
                  genny::DefaultRandom& rng,
                  metrics::Operation operation)
        : _onSession{onSession},
          _collection{collection},
          _rng{rng},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "Find", "Filter", rng)} {}
    // TODO: parse Find Options

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto ctx = _operation.start();
        auto cursor = (_onSession) ? _collection.find(session, std::move(filter), _options)
                                   : _collection.find(std::move(filter), _options);
        for (auto&& doc : cursor) {
            ctx.addDocuments(1);
            ctx.addBytes(doc.length());
        }
        ctx.success();
    }


private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::find _options;
    genny::DefaultRandom& _rng;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: insertMany
 *      OperationCommand:
 *        Documents:
 *        - { a : 1 }
 *        - { b : 1 }
 */
struct InsertManyOperation : public BaseOperation {

    InsertManyOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        genny::DefaultRandom& rng,
                        metrics::Operation operation)
        : _onSession{onSession}, _collection{collection}, _operation{operation}, _rng{rng} {
        auto documents = opNode["Documents"];
        if (!documents && !documents.IsSequence()) {
            throw InvalidConfigurationException(
                "'insertMany' expects a 'Documents' field of sequence type.");
        }
        for (auto&& document : documents) {
            _docExprs.push_back(Generators::document(document, _rng));
        }
        // TODO: parse insert options.
    }

    void run(mongocxx::client_session& session) override {
        for (auto&& docExpr : _docExprs) {
            auto doc = docExpr();
            _writeOps.push_back(std::move(doc));
        }
        auto ctx = _operation.start();
        (_onSession) ? _collection.insert_many(session, _writeOps, _options)
                     : _collection.insert_many(_writeOps, _options);
        ctx.success();
    }

private:
    mongocxx::collection _collection;
    const bool _onSession;
    std::vector<bsoncxx::document::view_or_value> _writeOps;
    mongocxx::options::insert _options;
    metrics::Operation _operation;
    genny::DefaultRandom& _rng;
    std::vector<DocumentGenerator> _docExprs;
};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: startTransaction
 *      OperationCommand:
 *        Options:
 *          WriteConcern:
 *            Level: majority
 *            Journal: true
 *          ReadConcern:
 *            Level: snapshot
 *          ReadPreference:
 *            ReadMode: primaryPreferred
 *            MaxStalenessSeconds: 1000
 */

struct StartTransactionOperation : public BaseOperation {

    StartTransactionOperation(YAML::Node opNode,
                              bool onSession,
                              mongocxx::collection collection,
                              genny::DefaultRandom& rng,
                              metrics::Operation operation) {
        if (!opNode.IsMap())
            return;
        if (opNode["Options"]) {
            _options = opNode["Options"].as<mongocxx::options::transaction>();
        }
    }

    void run(mongocxx::client_session& session) override {
        session.start_transaction(_options);
    }

private:
    mongocxx::options::transaction _options;
};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: commitTransaction
 */

struct CommitTransactionOperation : public BaseOperation {

    CommitTransactionOperation(YAML::Node opNode,
                               bool onSession,
                               mongocxx::collection collection,
                               genny::DefaultRandom& rng,
                               metrics::Operation operation) {}

    void run(mongocxx::client_session& session) override {
        session.commit_transaction();
    }
};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: setReadConcern
 *      OperationCommand:
 *        ReadConcern:
 *          Level: majority
 */

struct SetReadConcernOperation : public BaseOperation {

    static bool isValidReadConcernString(std::string_view rcString) {
        return (rcString == "local" || rcString == "majority" || rcString == "linearizable" ||
                rcString == "snapshot" || rcString == "available");
    }

    SetReadConcernOperation(YAML::Node opNode,
                            bool onSession,
                            mongocxx::collection collection,
                            genny::DefaultRandom& rng,
                            metrics::Operation operation)
        : _collection{collection} {
        if (!opNode["ReadConcern"]) {
            throw InvalidConfigurationException(
                "'setReadConcern' operation expects a 'ReadConcern' field.");
        }
        _readConcern = opNode["ReadConcern"].as<mongocxx::read_concern>();
    }

    void run(mongocxx::client_session& session) override {
        _collection.read_concern(_readConcern);
    }

private:
    mongocxx::collection _collection;
    mongocxx::read_concern _readConcern;
};

/**
 * Example usage:
 *    Operations:
 *    - OperationName: drop
 *      OperationCommand:
 *        OnSession: false
 *        Options:
 *          WriteConcern:
 *            Level: majority
 */
struct DropOperation : public BaseOperation {

    DropOperation(YAML::Node opNode,
                  bool onSession,
                  mongocxx::collection collection,
                  genny::DefaultRandom& rng,
                  metrics::Operation operation)
        : _onSession{onSession}, _collection{collection}, _operation{operation} {
        if (!opNode)
            return;
        if (opNode["Options"] && opNode["Options"]["WriteConcern"]) {
            _wc = opNode["Options"]["WriteConcern"].as<mongocxx::write_concern>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto ctx = _operation.start();
        (_onSession) ? _collection.drop(session, _wc) : _collection.drop(_wc);
        ctx.success();
    }

private:
    mongocxx::collection _collection;
    const bool _onSession;
    std::vector<bsoncxx::document::value> _writeOps;
    metrics::Operation _operation;
    mongocxx::write_concern _wc;
};

// Maps the yaml 'OperationName' string to the appropriate constructor of 'BaseOperation' type.
std::unordered_map<std::string, OpCallback&> opConstructors = {
    {"bulkWrite", baseCallback<BaseOperation, OpCallback, BulkWriteOperation>},
    {"countDocuments", baseCallback<BaseOperation, OpCallback, CountDocumentsOperation>},
    {"find", baseCallback<BaseOperation, OpCallback, FindOperation>},
    {"insertMany", baseCallback<BaseOperation, OpCallback, InsertManyOperation>},
    {"startTransaction", baseCallback<BaseOperation, OpCallback, StartTransactionOperation>},
    {"commitTransaction", baseCallback<BaseOperation, OpCallback, CommitTransactionOperation>},
    {"setReadConcern", baseCallback<BaseOperation, OpCallback, SetReadConcernOperation>},
    {"drop", baseCallback<BaseOperation, OpCallback, DropOperation>},
    {"insertOne", baseCallback<BaseOperation, OpCallback, InsertOneOperation>},
    {"deleteOne", baseCallback<BaseOperation, OpCallback, DeleteOneOperation>},
    {"deleteMany", baseCallback<BaseOperation, OpCallback, DeleteManyOperation>},
    {"updateOne", baseCallback<BaseOperation, OpCallback, UpdateOneOperation>},
    {"updateMany", baseCallback<BaseOperation, OpCallback, UpdateManyOperation>},
    {"replaceOne", baseCallback<BaseOperation, OpCallback, ReplaceOneOperation>}};
};  // namespace

namespace genny::actor {

struct CrudActor::PhaseConfig {
    mongocxx::collection collection;
    ExecutionStrategy::RunOptions options;
    std::vector<std::unique_ptr<BaseOperation>> operations;
    ExecutionStrategy strategy;

    PhaseConfig(PhaseContext& phaseContext,
                genny::DefaultRandom& rng,
                const mongocxx::database& db,
                ActorContext& actorContext,
                ActorId id)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          strategy{actorContext.operation("Crud", id)},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {

        auto addOperation = [&](YAML::Node node) {
            auto yamlCommand = node["OperationCommand"];
            auto opName = node["OperationName"].as<std::string>();
            auto onSession = false;
            if (yamlCommand) {
                onSession = yamlCommand["OnSession"] && yamlCommand["OnSession"].as<bool>();
            }

            // Grab the appropriate Operation struct defined by 'OperationName'.
            auto op = opConstructors.find(opName);
            if (op == opConstructors.end()) {
                throw InvalidConfigurationException("Operation '" + opName +
                                                    "' not supported in Crud Actor.");
            }
            auto createOperation = op->second;
            return createOperation(
                yamlCommand, onSession, collection, rng, actorContext.operation(opName, id));
        };

        operations = phaseContext.getPlural<std::unique_ptr<BaseOperation>>(
            "Operation", "Operations", addOperation);
    }
};

void CrudActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto session = _client->start_session();
            config->strategy.run(
                [&](metrics::OperationContext&) {
                    for (auto&& op : config->operations) {
                        op->run(session);
                    }
                },
                config->options);
        }
    }
}

CrudActor::CrudActor(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _client{std::move(context.client())},
      _loop{context,
            _rng,
            (*_client)[context.get<std::string>("Database")],
            context,
            CrudActor::id()} {}

namespace {
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}
}  // namespace genny::actor
