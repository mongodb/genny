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
#include <utility>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/json.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/RetryStrategy.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;

namespace YAML {

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
struct convert<mongocxx::options::update> {
    static Node encode(const mongocxx::options::update& rhs) {
        return {};
    }
    static bool decode(const Node& node, mongocxx::options::update& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        if (node["WriteConcern"]) {
            rhs.write_concern(node["WriteConcern"].as<mongocxx::write_concern>());
        }
        /* not supported yet
        if(node["array_filters"]) {
            rhs.array_filters(node["array_filters"].as<genny::DocumentGenerator>());
        }
        */
        if (node["Upsert"]) {
            rhs.upsert(node["Upsert"].as<bool>());
        }
        /* not supported yet
        if(node["collation"]) {
            rhs.collation(node["collation"].as<genny::DocumentGenerator>());
        }
        */
        if (node["BypassDocumentValidation"]) {
            rhs.bypass_document_validation(node["BypassDocumentValidation"].as<bool>());
        }

        return true;
    }
};

template <>
struct convert<mongocxx::options::insert> {
    static Node encode(const mongocxx::options::insert& rhs) {
        return {};
    }
    static bool decode(const Node& node, mongocxx::options::insert& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        if (node["Ordered"]) {
            rhs.ordered(node["Ordered"].as<bool>());
        }
        if (node["BypassDocumentValidation"]) {
            rhs.bypass_document_validation(node["BypassDocumentValidation"].as<bool>());
        }
        if (node["WriteConcern"]) {
            rhs.write_concern(node["WriteConcern"].as<mongocxx::write_concern>());
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

enum class ThrowMode {
    kSwallow,
    kRethrow,
};

ThrowMode decodeThrowMode(YAML::Node operation, PhaseContext& phaseContext) {
    static const char* key = "ThrowOnFailure";

    // look in operation otherwise fallback to phasecontext
    // we really need to kill YAML::Node and only use ConfigNode...
    bool throwOnFailure = operation[key] ? operation[key].as<bool>()
                                         : phaseContext.get<bool, false>(key).value_or(true);
    return throwOnFailure ? ThrowMode::kRethrow : ThrowMode::kSwallow;
}

bsoncxx::document::value emptyDoc = bsoncxx::from_json("{}");

struct BaseOperation {
    ThrowMode throwMode;

    using MaybeDoc = std::optional<bsoncxx::document::value>;

    explicit BaseOperation(PhaseContext& phaseContext, YAML::Node operation)
        : throwMode{decodeThrowMode(operation, phaseContext)} {}

    template <typename F>
    void doBlock(metrics::Operation& op, F&& f) {
        MaybeDoc info = std::nullopt;
        auto ctx = op.start();
        try {
            info = f(ctx);
        } catch (const mongocxx::operation_exception& x) {
            if (throwMode == ThrowMode::kRethrow) {
                ctx.failure();
                BOOST_THROW_EXCEPTION(MongoException(x, info ? info->view() : emptyDoc.view()));
            }
        }
        ctx.success();
    }

    virtual void run(mongocxx::client_session& session) = 0;
    virtual ~BaseOperation() = default;
};

using OpCallback = std::function<std::unique_ptr<BaseOperation>(
    YAML::Node, bool, mongocxx::collection, metrics::Operation, PhaseContext& context, ActorId id)>;

struct WriteOperation : public BaseOperation {
    WriteOperation(PhaseContext& phaseContext, YAML::Node operation)
        : BaseOperation(phaseContext, operation) {}
    virtual mongocxx::model::write getModel() = 0;
};

using WriteOpCallback = std::function<std::unique_ptr<WriteOperation>(
    YAML::Node, bool, mongocxx::collection, metrics::Operation, PhaseContext&, ActorId)>;

namespace {

auto createGenerator(YAML::Node source,
                     const std::string& opType,
                     const std::string& key,
                     PhaseContext& context,
                     ActorId id) {
    auto doc = source[key];
    if (!doc) {
        std::stringstream msg;
        msg << "'" << opType << "' expects a '" << key << "' field.";
        BOOST_THROW_EXCEPTION(InvalidConfigurationException(msg.str()));
    }
    return context.createDocumentGenerator(id, doc);
}

}  // namespace

struct CreateIndexOperation : public BaseOperation {
    mongocxx::collection _collection;
    metrics::Operation _operation;
    DocumentGenerator _keysGenerator;
    DocumentGenerator _indexOptionsGenerator;
    mongocxx::options::index_view _operationOptions;
    // TODO: convert from yaml

    bool _onSession;

    CreateIndexOperation(YAML::Node opNode,
                         bool onSession,
                         mongocxx::collection collection,
                         metrics::Operation operation,
                         PhaseContext& context,
                         ActorId id)
        : BaseOperation(context, opNode),
          _collection(std::move(collection)),
          _operation{operation},
          _onSession{onSession},
          _keysGenerator{createGenerator(opNode, "createIndex", "Keys", context, id)},
          _indexOptionsGenerator{
              createGenerator(opNode, "createIndex", "IndexOptions", context, id)} {}


    void run(mongocxx::client_session& session) override {
        auto keys = _keysGenerator();
        auto indexOptions = _indexOptionsGenerator();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            _onSession
                ? _collection.create_index(keys.view(), indexOptions.view(), _operationOptions)
                : _collection.create_index(
                      session, keys.view(), indexOptions.view(), _operationOptions);
            return std::make_optional(std::move(keys));
        });
    }
};

struct InsertOneOperation : public WriteOperation {
    InsertOneOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _options{opNode["OperationOptions"].as<mongocxx::options::insert>(
              mongocxx::options::insert{})},
          _docExpr{createGenerator(opNode, "insertOne", "Document", context, id)} {

        // TODO: parse insert options.
    }

    mongocxx::model::write getModel() override {
        auto document = _docExpr();
        return mongocxx::model::insert_one{std::move(document)};
    }

    void run(mongocxx::client_session& session) override {
        auto document = _docExpr();
        auto size = document.view().length();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            (_onSession) ? _collection.insert_one(session, std::move(document), _options)
                         : _collection.insert_one(std::move(document), _options);
            ctx.addDocuments(1);
            ctx.addBytes(size);
            return std::make_optional(std::move(document));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _docExpr;
    metrics::Operation _operation;
    mongocxx::options::insert _options;
};


struct UpdateOneOperation : public WriteOperation {
    UpdateOneOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "updateOne", "Filter", context, id)},
          _updateExpr{createGenerator(opNode, "updateOne", "Update", context, id)},
          _options{opNode["OperationOptions"].as<mongocxx::options::update>(
              mongocxx::options::update{})} {}
    // TODO: parse update options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        return mongocxx::model::update_one{std::move(filter), std::move(update)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.update_one(session, std::move(filter), std::move(update), _options)
                : _collection.update_one(std::move(filter), std::move(update), _options);
            if (result) {
                ctx.addDocuments(result->modified_count());
            }
            // pick an 'info' document...either one makes sense
            return std::make_optional(std::move(update));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filterExpr;
    DocumentGenerator _updateExpr;
    metrics::Operation _operation;
    mongocxx::options::update _options;
};

struct UpdateManyOperation : public WriteOperation {
    UpdateManyOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "updateMany", "Filter", context, id)},
          _updateExpr{createGenerator(opNode, "updateMany", "Update", context, id)} {}

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        return mongocxx::model::update_many{std::move(filter), std::move(update)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto update = _updateExpr();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.update_many(session, std::move(filter), std::move(update), _options)
                : _collection.update_many(std::move(filter), std::move(update), _options);
            if (result) {
                ctx.addDocuments(result->modified_count());
            }
            return std::make_optional(std::move(update));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filterExpr;
    DocumentGenerator _updateExpr;
    metrics::Operation _operation;
    mongocxx::options::update _options;
};

struct DeleteOneOperation : public WriteOperation {
    DeleteOneOperation(YAML::Node opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr(createGenerator(opNode, "deleteOne", "Filter", context, id)) {}
    // TODO: parse delete options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        return mongocxx::model::delete_one{std::move(filter)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.delete_one(session, std::move(filter), _options)
                : _collection.delete_one(std::move(filter), _options);
            if (result) {
                ctx.addDocuments(result->deleted_count());
            }
            return std::make_optional(std::move(filter));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
    mongocxx::options::delete_options _options;
};

struct DeleteManyOperation : public WriteOperation {
    DeleteManyOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "deleteMany", "Filter", context, id)} {}
    // TODO: parse delete options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        return mongocxx::model::delete_many{std::move(filter)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto results = (_onSession)
                ? _collection.delete_many(session, std::move(filter), _options)
                : _collection.delete_many(std::move(filter), _options);
            if (results) {
                ctx.addDocuments(results->deleted_count());
            }
            return std::make_optional(std::move(filter));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
    mongocxx::options::delete_options _options;
};

struct ReplaceOneOperation : public WriteOperation {
    ReplaceOneOperation(YAML::Node opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "replaceOne", "Filter", context, id)},
          _replacementExpr{createGenerator(opNode, "replaceOne", "Replacement", context, id)} {}

    // TODO: parse replace options.

    mongocxx::model::write getModel() override {
        auto filter = _filterExpr();
        auto replacement = _replacementExpr();
        return mongocxx::model::replace_one{std::move(filter), std::move(replacement)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto replacement = _replacementExpr();
        auto size = replacement.view().length();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.replace_one(
                      session, std::move(filter), std::move(replacement), _options)
                : _collection.replace_one(std::move(filter), std::move(replacement), _options);

            if (result) {
                ctx.addDocuments(result->modified_count());
            }
            ctx.addBytes(size);

            return std::make_optional(std::move(replacement));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filterExpr;
    DocumentGenerator _replacementExpr;
    metrics::Operation _operation;
    mongocxx::options::replace _options;
};

template <class P, class C, class O>
C baseCallback = [](YAML::Node opNode,
                    bool onSession,
                    mongocxx::collection collection,
                    metrics::Operation operation,
                    PhaseContext& context,
                    ActorId id) -> std::unique_ptr<P> {
    return std::make_unique<O>(opNode, onSession, collection, operation, context, id);
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
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        auto writeOpsYaml = opNode["WriteOperations"];
        if (!writeOpsYaml.IsSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'bulkWrite' requires a 'WriteOperations' node of sequence type."));
        }
        for (auto&& writeOp : writeOpsYaml) {
            createOps(writeOp, context, id);
        }
        if (opNode["Options"]) {
            _options = opNode["Options"].as<mongocxx::options::bulk_write>();
        }
    }

    void createOps(const YAML::Node& writeOp, PhaseContext& context, ActorId id) {
        auto writeCommand = writeOp["WriteCommand"].as<std::string>();
        auto writeOpConstructor = bulkWriteConstructors.find(writeCommand);
        if (writeOpConstructor == bulkWriteConstructors.end()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "WriteCommand '" + writeCommand + "' not supported in bulkWrite operations."));
        }
        auto createWriteOp = writeOpConstructor->second;
        _writeOps.push_back(
            createWriteOp(writeOp, _onSession, _collection, _operation, context, id));
    }

    void run(mongocxx::client_session& session) override {
        auto bulk = (_onSession) ? _collection.create_bulk_write(session, _options)
                                 : _collection.create_bulk_write(_options);
        for (auto&& op : _writeOps) {
            auto writeModel = op->getModel();
            bulk.append(writeModel);
        }

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = bulk.execute();

            size_t docs = 0;
            if (result) {
                docs += result->modified_count();
                docs += result->deleted_count();
                docs += result->inserted_count();
                docs += result->upserted_count();
                ctx.addDocuments(docs);
            }
            // no viable option
            return std::nullopt;
        });
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
                            metrics::Operation operation,
                            PhaseContext& context,
                            ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "Count", "Filter", context, id)} {
        if (opNode["Options"]) {
            _options = opNode["Options"].as<mongocxx::options::count>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto count = (_onSession)
                ? _collection.count_documents(session, std::move(filter), _options)
                : _collection.count_documents(std::move(filter), _options);
            ctx.addDocuments(count);
            return std::make_optional(std::move(filter));
        });
    }


private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::count _options;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
};

struct FindOperation : public BaseOperation {
    FindOperation(YAML::Node opNode,
                  bool onSession,
                  mongocxx::collection collection,
                  metrics::Operation operation,
                  PhaseContext& context,
                  ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "Find", "Filter", context, id)} {}
    // TODO: parse Find Options

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto cursor = (_onSession) ? _collection.find(session, std::move(filter), _options)
                                       : _collection.find(std::move(filter), _options);
            for (auto&& doc : cursor) {
                ctx.addDocuments(1);
                ctx.addBytes(doc.length());
            }
            return std::make_optional(std::move(filter));
        });
    }


private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::find _options;
    DocumentGenerator _filterExpr;
    metrics::Operation _operation;
};

struct FindOneAndUpdateOperation : public BaseOperation {
    FindOneAndUpdateOperation(YAML::Node opNode,
                              bool onSession,
                              mongocxx::collection collection,
                              metrics::Operation operation,
                              PhaseContext& context,
                              ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filterExpr{createGenerator(opNode, "FindOneAndUpdate", "Filter", context, id)},
          _updateExpr{createGenerator(opNode, "FindOneAndUpdate", "Update", context, id)} {}

    void run(mongocxx::client_session& session) override {
        auto filter = _filterExpr();
        auto update = _updateExpr();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            // TODO: remove moves from calls into _collection methods
            auto result = (_onSession)
                ? _collection.find_one_and_update(session, filter.view(), update.view(), _options)
                : _collection.find_one_and_update(filter.view(), update.view(), _options);
            if (result) {
                ctx.addDocuments(1);
                ctx.addBytes(result->view().length());
            }
            return std::make_optional(std::move(filter));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    // TODO: parse _options
    mongocxx::options::find_one_and_update _options;
    DocumentGenerator _filterExpr;
    DocumentGenerator _updateExpr;
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
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        auto documents = opNode["Documents"];
        if (!documents && !documents.IsSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'insertMany' expects a 'Documents' field of sequence type."));
        }
        for (auto&& document : documents) {
            _docExprs.push_back(context.createDocumentGenerator(id, document));
        }
        // TODO: parse insert options.
    }

    void run(mongocxx::client_session& session) override {
        size_t bytes = 0;
        for (auto&& docExpr : _docExprs) {
            auto doc = docExpr();
            bytes += doc.view().length();
            _writeOps.emplace_back(std::move(doc));
        }

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession) ? _collection.insert_many(session, _writeOps, _options)
                                       : _collection.insert_many(_writeOps, _options);

            ctx.addBytes(bytes);
            if (result) {
                ctx.addDocuments(result->inserted_count());
            }
            return std::nullopt;
        });
    }

private:
    mongocxx::collection _collection;
    const bool _onSession;
    std::vector<bsoncxx::document::view_or_value> _writeOps;
    mongocxx::options::insert _options;
    metrics::Operation _operation;
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
                              metrics::Operation operation,
                              PhaseContext& context,
                              ActorId id)
        : BaseOperation(context, opNode), _operation{operation} {
        if (!opNode.IsMap())
            return;
        if (opNode["Options"]) {
            _options = opNode["Options"].as<mongocxx::options::transaction>();
        }
    }

    void run(mongocxx::client_session& session) override {
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            session.start_transaction(_options);
            return std::nullopt;
        });
    }

private:
    mongocxx::options::transaction _options;
    metrics::Operation _operation;
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
                               metrics::Operation operation,
                               PhaseContext& context,
                               ActorId id)
        : BaseOperation(context, opNode), _operation{operation} {}

    void run(mongocxx::client_session& session) override {
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            session.commit_transaction();
            return std::nullopt;
        });
    }

private:
    metrics::Operation _operation;
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
    SetReadConcernOperation(YAML::Node opNode,
                            bool onSession,
                            mongocxx::collection collection,
                            metrics::Operation operation,
                            PhaseContext& context,
                            ActorId id)
        : BaseOperation(context, opNode),
          _collection{std::move(collection)},
          _operation{operation} {
        if (!opNode["ReadConcern"]) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'setReadConcern' operation expects a 'ReadConcern' field."));
        }
        _readConcern = opNode["ReadConcern"].as<mongocxx::read_concern>();
    }

    void run(mongocxx::client_session& session) override {
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            _collection.read_concern(_readConcern);
            return std::nullopt;
        });
    }

private:
    mongocxx::collection _collection;
    mongocxx::read_concern _readConcern;
    metrics::Operation _operation;
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
                  metrics::Operation operation,
                  PhaseContext& context,
                  ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        if (!opNode)
            return;
        if (opNode["Options"] && opNode["Options"]["WriteConcern"]) {
            _wc = opNode["Options"]["WriteConcern"].as<mongocxx::write_concern>();
        }
    }

    void run(mongocxx::client_session& session) override {
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            (_onSession) ? _collection.drop(session, _wc) : _collection.drop(_wc);
            // ideally return something indicating collection name
            return std::nullopt;
        });
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
    {"createIndex", baseCallback<BaseOperation, OpCallback, CreateIndexOperation>},
    {"find", baseCallback<BaseOperation, OpCallback, FindOperation>},
    {"findOneAndUpdate", baseCallback<BaseOperation, OpCallback, FindOneAndUpdateOperation>},
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
    RetryStrategy::Options options;
    std::vector<std::unique_ptr<BaseOperation>> operations;
    metrics::Operation metrics;

    PhaseConfig(PhaseContext& phaseContext,
                const mongocxx::database& db,
                ActorContext& actorContext,
                ActorId id)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          metrics{actorContext.operation("Crud", id)} {
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
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "Operation '" + opName + "' not supported in Crud Actor."));
            }
            auto createOperation = op->second;
            return createOperation(yamlCommand,
                                   onSession,
                                   collection,
                                   actorContext.operation(opName, id),
                                   phaseContext,
                                   id);
        };

        operations = phaseContext.getPlural<std::unique_ptr<BaseOperation>>(
            "Operation", "Operations", addOperation);
    }
};

void CrudActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto metricsContext = config->metrics.start();

            auto session = _client->start_session();
            for (auto&& op : config->operations) {
                op->run(session);
            }

            metricsContext.success();
        }
    }
}

CrudActor::CrudActor(genny::ActorContext& context)
    : Actor(context),
      _client{std::move(context.client())},
      _loop{context, (*_client)[context.get<std::string>("Database")], context, CrudActor::id()} {}

namespace {
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}
}  // namespace genny::actor
