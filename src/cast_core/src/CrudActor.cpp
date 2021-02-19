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

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/json.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;

namespace YAML {

template <>
struct convert<mongocxx::options::aggregate> {
    using AggregateOptions = mongocxx::options::aggregate;
    static Node encode(const AggregateOptions& rhs) {
        return {};
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
        return {};
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
        return {};
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
struct convert<mongocxx::options::estimated_document_count> {
    using CountOptions = mongocxx::options::estimated_document_count;
    static Node encode(const CountOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, CountOptions& rhs) {
        if (!node.IsMap()) {
            return false;
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
struct convert<mongocxx::options::insert> {
    static Node encode(const mongocxx::options::insert& rhs) {
        return {};
    }
    static bool decode(const Node& node, mongocxx::options::insert& rhs) {
        if (!node.IsMap()) {
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
        return {};
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
    kSwallowAndRecord,  // Record failed operations but don't throw.
};

ThrowMode decodeThrowMode(const Node& operation, PhaseContext& phaseContext) {
    static const char* throwKey = "ThrowOnFailure";
    // TODO: TIG-2805 Remove this mode once the underlying drivers issue is addressed.
    static const char* ignoreKey = "RecordFailure";

    bool throwOnFailure = operation[throwKey] ? operation[throwKey].to<bool>()
                                              : phaseContext[throwKey].maybe<bool>().value_or(true);
    bool ignoreFailure = operation[ignoreKey]
        ? operation[ignoreKey].to<bool>()
        : phaseContext[ignoreKey].maybe<bool>().value_or(false);
    if (ignoreFailure) {
        return ThrowMode::kSwallowAndRecord;
    }
    return throwOnFailure ? ThrowMode::kRethrow : ThrowMode::kSwallow;
}

bsoncxx::document::value emptyDoc = bsoncxx::from_json("{}");


// A large number of subclasses have
// - metrics::Operation
// - mongocxx::collection
// - bool (_onSession)
// it may make sense to add a CollectionAwareBaseOperation
// or something intermediate class to eliminate some
// duplication.
struct BaseOperation {
    ThrowMode throwMode;

    using MaybeDoc = std::optional<bsoncxx::document::value>;

    explicit BaseOperation(PhaseContext& phaseContext, const Node& operation)
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
            } else if (throwMode == ThrowMode::kSwallowAndRecord) {
                // Record the failure but don't throw.
                ctx.failure();
                return;
            }
        }
        ctx.success();
    }

    virtual void run(mongocxx::client_session& session) = 0;
    virtual ~BaseOperation() = default;
};

using OpCallback = std::function<std::unique_ptr<BaseOperation>(const Node&,
                                                                bool,
                                                                mongocxx::collection,
                                                                metrics::Operation,
                                                                PhaseContext& context,
                                                                ActorId id)>;

struct WriteOperation : public BaseOperation {
    WriteOperation(PhaseContext& phaseContext, const Node& operation)
        : BaseOperation(phaseContext, operation) {}
    virtual mongocxx::model::write getModel() = 0;
};

using WriteOpCallback = std::function<std::unique_ptr<WriteOperation>(
    const Node&, bool, mongocxx::collection, metrics::Operation, PhaseContext&, ActorId)>;

// Not technically "crud" but it was easy to add and made
// a few of the tests easier to write (by allowing inserts
// to fail due to index constraint-violations.
struct CreateIndexOperation : public BaseOperation {
    mongocxx::collection _collection;
    metrics::Operation _operation;
    DocumentGenerator _keys;
    DocumentGenerator _indexOptions;
    mongocxx::options::index_view _operationOptions;

    bool _onSession;

    CreateIndexOperation(const Node& opNode,
                         bool onSession,
                         mongocxx::collection collection,
                         metrics::Operation operation,
                         PhaseContext& context,
                         ActorId id)
        : BaseOperation(context, opNode),
          _collection(std::move(collection)),
          _operation{operation},
          _onSession{onSession},
          _keys{opNode["Keys"].to<DocumentGenerator>(context, id)},
          _indexOptions{opNode["IndexOptions"].to<DocumentGenerator>(context, id)} {}


    void run(mongocxx::client_session& session) override {
        auto keys = _keys();
        auto indexOptions = _indexOptions();

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
    InsertOneOperation(const Node& opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _options{opNode["OperationOptions"].maybe<mongocxx::options::insert>().value_or(
              mongocxx::options::insert{})},
          _document{opNode["Document"].to<DocumentGenerator>(context, id)} {}

    mongocxx::model::write getModel() override {
        auto document = _document();
        return mongocxx::model::insert_one{std::move(document)};
    }

    void run(mongocxx::client_session& session) override {
        auto document = _document();
        auto size = document.view().length();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            (_onSession) ? _collection.insert_one(session, document.view(), _options)
                         : _collection.insert_one(document.view(), _options);
            ctx.addDocuments(1);
            ctx.addBytes(size);
            return std::make_optional(std::move(document));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _document;
    metrics::Operation _operation;
    mongocxx::options::insert _options;
};


struct UpdateOneOperation : public WriteOperation {
    UpdateOneOperation(const Node& opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)},
          _update{opNode["Update"].to<DocumentGenerator>(context, id)} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        auto update = _update();
        return mongocxx::model::update_one{std::move(filter), std::move(update)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        auto update = _update();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.update_one(session, filter.view(), update.view(), _options)
                : _collection.update_one(filter.view(), update.view(), _options);
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
    DocumentGenerator _filter;
    DocumentGenerator _update;
    metrics::Operation _operation;
    mongocxx::options::update _options;
};

struct UpdateManyOperation : public WriteOperation {
    UpdateManyOperation(const Node& opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)},
          _update{opNode["Update"].to<DocumentGenerator>(context, id)} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        auto update = _update();
        return mongocxx::model::update_many{std::move(filter), std::move(update)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        auto update = _update();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.update_many(session, filter.view(), update.view(), _options)
                : _collection.update_many(filter.view(), update.view(), _options);
            if (result) {
                ctx.addDocuments(result->modified_count());
            }
            return std::make_optional(std::move(update));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filter;
    DocumentGenerator _update;
    metrics::Operation _operation;
    mongocxx::options::update _options;
};

struct DeleteOneOperation : public WriteOperation {
    DeleteOneOperation(const Node& opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter(opNode["Filter"].to<DocumentGenerator>(context, id)) {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        return mongocxx::model::delete_one{std::move(filter)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession) ? _collection.delete_one(session, filter.view(), _options)
                                       : _collection.delete_one(filter.view(), _options);
            if (result) {
                ctx.addDocuments(result->deleted_count());
            }
            return std::make_optional(std::move(filter));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filter;
    metrics::Operation _operation;
    mongocxx::options::delete_options _options;
};

struct DeleteManyOperation : public WriteOperation {
    DeleteManyOperation(const Node& opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        return mongocxx::model::delete_many{std::move(filter)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto results = (_onSession) ? _collection.delete_many(session, filter.view(), _options)
                                        : _collection.delete_many(filter.view(), _options);
            if (results) {
                ctx.addDocuments(results->deleted_count());
            }
            return std::make_optional(std::move(filter));
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    DocumentGenerator _filter;
    metrics::Operation _operation;
    mongocxx::options::delete_options _options;
};

struct ReplaceOneOperation : public WriteOperation {
    ReplaceOneOperation(const Node& opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : WriteOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)},
          _replacement{opNode["Replacement"].to<DocumentGenerator>(context, id)} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        auto replacement = _replacement();
        return mongocxx::model::replace_one{std::move(filter), std::move(replacement)};
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        auto replacement = _replacement();
        auto size = replacement.view().length();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.replace_one(session, filter.view(), replacement.view(), _options)
                : _collection.replace_one(filter.view(), replacement.view(), _options);

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
    DocumentGenerator _filter;
    DocumentGenerator _replacement;
    metrics::Operation _operation;
    mongocxx::options::replace _options;
};

template <class P, class C, class O>
C baseCallback = [](const Node& opNode,
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

    BulkWriteOperation(const Node& opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        auto& writeOpsYaml = opNode["WriteOperations"];
        if (!writeOpsYaml.isSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'bulkWrite' requires a 'WriteOperations' node of sequence type."));
        }
        for (const auto& [k, writeOp] : writeOpsYaml) {
            createOps(writeOp, context, id);
        }
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::bulk_write>();
        }
    }

    void createOps(const Node& writeOp, PhaseContext& context, ActorId id) {
        auto writeCommand = writeOp["WriteCommand"].to<std::string>();
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
    CountDocumentsOperation(const Node& opNode,
                            bool onSession,
                            mongocxx::collection collection,
                            metrics::Operation operation,
                            PhaseContext& context,
                            ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)} {
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::count>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto count = (_onSession)
                ? _collection.count_documents(session, filter.view(), _options)
                : _collection.count_documents(filter.view(), _options);
            ctx.addDocuments(count);
            return std::make_optional(std::move(filter));
        });
    }


private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::count _options;
    DocumentGenerator _filter;
    metrics::Operation _operation;
};

struct EstimatedDocumentCountOperation : public BaseOperation {
    EstimatedDocumentCountOperation(const Node& opNode,
                                    bool onSession,
                                    mongocxx::collection collection,
                                    metrics::Operation operation,
                                    PhaseContext& context,
                                    ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::estimated_document_count>();
        }
        if (onSession) {
            // Technically the docs don't explicitly mention this but the C++ driver doesn't
            // let you specify a session. In fact count operations operations don't work inside
            // a transaction so maybe CountOperation should fail in the same way if _onSession
            // is set?
            //
            // https://docs.mongodb.com/manual/reference/method/db.collection.estimatedDocumentCount/
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("Cannot run EstimatedDocumentCount on a session"));
        }
    }

    void run(mongocxx::client_session& session) override {
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto count = _collection.estimated_document_count(_options);
            ctx.addDocuments(count);
            return std::nullopt;
        });
    }


private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::estimated_document_count _options;
    metrics::Operation _operation;
};

struct FindOperation : public BaseOperation {
    FindOperation(const Node& opNode,
                  bool onSession,
                  mongocxx::collection collection,
                  metrics::Operation operation,
                  PhaseContext& context,
                  ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)} {
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::find>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto cursor = (_onSession) ? _collection.find(session, filter.view(), _options)
                                       : _collection.find(filter.view(), _options);
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
    DocumentGenerator _filter;
    metrics::Operation _operation;
};

struct FindOneOperation : public BaseOperation {
    FindOneOperation(const Node& opNode,
                     bool onSession,
                     mongocxx::collection collection,
                     metrics::Operation operation,
                     PhaseContext& context,
                     ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)} {
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::find>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession) ? _collection.find_one(session, filter.view(), _options)
                                       : _collection.find_one(filter.view(), _options);
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
    mongocxx::options::find _options;
    DocumentGenerator _filter;
    metrics::Operation _operation;
};

struct FindOneAndUpdateOperation : public BaseOperation {
    FindOneAndUpdateOperation(const Node& opNode,
                              bool onSession,
                              mongocxx::collection collection,
                              metrics::Operation operation,
                              PhaseContext& context,
                              ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)},
          _update{opNode["Update"].to<DocumentGenerator>(context, id)} {}

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        auto update = _update();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
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
    mongocxx::options::find_one_and_update _options;
    DocumentGenerator _filter;
    DocumentGenerator _update;
    metrics::Operation _operation;
};

struct FindOneAndDeleteOperation : public BaseOperation {
    FindOneAndDeleteOperation(const Node& opNode,
                              bool onSession,
                              mongocxx::collection collection,
                              metrics::Operation operation,
                              PhaseContext& context,
                              ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)} {}

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.find_one_and_delete(session, filter.view(), _options)
                : _collection.find_one_and_delete(filter.view(), _options);
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
    mongocxx::options::find_one_and_delete _options;
    DocumentGenerator _filter;
    metrics::Operation _operation;
};

struct FindOneAndReplaceOperation : public BaseOperation {
    FindOneAndReplaceOperation(const Node& opNode,
                               bool onSession,
                               mongocxx::collection collection,
                               metrics::Operation operation,
                               PhaseContext& context,
                               ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _filter{opNode["Filter"].to<DocumentGenerator>(context, id)},
          _replacement{opNode["Replacement"].to<DocumentGenerator>(context, id)} {}

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        auto replacement = _replacement();
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession)
                ? _collection.find_one_and_replace(
                      session, filter.view(), replacement.view(), _options)
                : _collection.find_one_and_replace(filter.view(), replacement.view(), _options);
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
    mongocxx::options::find_one_and_replace _options;
    DocumentGenerator _filter;
    DocumentGenerator _replacement;
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

    InsertManyOperation(const Node& opNode,
                        bool onSession,
                        mongocxx::collection collection,
                        metrics::Operation operation,
                        PhaseContext& context,
                        ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        auto& documents = opNode["Documents"];
        if (!documents && !documents.isSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'insertMany' expects a 'Documents' field of sequence type."));
        }
        for (auto&& [k, document] : documents) {
            _docExprs.push_back(document.to<DocumentGenerator>(context, id));
        }
    }

    void run(mongocxx::client_session& session) override {
        std::vector<bsoncxx::document::view_or_value> writeOps;
        size_t bytes = 0;
        for (auto&& docExpr : _docExprs) {
            auto doc = docExpr();
            bytes += doc.view().length();
            writeOps.emplace_back(std::move(doc));
        }

        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto result = (_onSession) ? _collection.insert_many(session, writeOps, _options)
                                       : _collection.insert_many(writeOps, _options);

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

    StartTransactionOperation(const Node& opNode,
                              bool onSession,
                              mongocxx::collection collection,
                              metrics::Operation operation,
                              PhaseContext& context,
                              ActorId id)
        : BaseOperation(context, opNode), _operation{operation} {
        if (!opNode.isMap())
            return;
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::transaction>();
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

    CommitTransactionOperation(const Node& opNode,
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
    SetReadConcernOperation(const Node& opNode,
                            bool onSession,
                            mongocxx::collection collection,
                            metrics::Operation operation,
                            PhaseContext& context,
                            ActorId id)
        : BaseOperation(context, opNode),
          _collection{std::move(collection)},
          _operation{operation} {
        _readConcern = opNode["ReadConcern"].to<mongocxx::read_concern>();
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
    DropOperation(const Node& opNode,
                  bool onSession,
                  mongocxx::collection collection,
                  metrics::Operation operation,
                  PhaseContext& context,
                  ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation} {
        if (auto& c = opNode["Options"]["WriteConcern"]) {
            _wc = c.to<mongocxx::write_concern>();
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
    metrics::Operation _operation;
    mongocxx::write_concern _wc;
};

// Maps the yaml 'OperationName' string to the appropriate constructor of 'BaseOperation' type.
std::unordered_map<std::string, OpCallback&> opConstructors = {
    {"bulkWrite", baseCallback<BaseOperation, OpCallback, BulkWriteOperation>},
    {"countDocuments", baseCallback<BaseOperation, OpCallback, CountDocumentsOperation>},
    {"estimatedDocumentCount",
     baseCallback<BaseOperation, OpCallback, EstimatedDocumentCountOperation>},
    {"createIndex", baseCallback<BaseOperation, OpCallback, CreateIndexOperation>},
    {"find", baseCallback<BaseOperation, OpCallback, FindOperation>},
    {"findOne", baseCallback<BaseOperation, OpCallback, FindOneOperation>},
    {"findOneAndUpdate", baseCallback<BaseOperation, OpCallback, FindOneAndUpdateOperation>},
    {"findOneAndDelete", baseCallback<BaseOperation, OpCallback, FindOneAndDeleteOperation>},
    {"findOneAndReplace", baseCallback<BaseOperation, OpCallback, FindOneAndReplaceOperation>},
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

// Equivalent to `nvl(phase[Database], actor[Database])`.
std::string getDbName(const PhaseContext& phaseContext) {
    auto phaseDb = phaseContext["Database"].maybe<std::string>();
    auto actorDb = phaseContext.actor()["Database"].maybe<std::string>();
    if (!phaseDb && !actorDb) {
        BOOST_THROW_EXCEPTION(
            InvalidConfigurationException("Must give Database in Phase or Actor block."));
    }
    return phaseDb ? *phaseDb : *actorDb;
}

}  // namespace

namespace genny::actor {

struct CrudActor::PhaseConfig {
    mongocxx::collection collection;
    std::vector<std::unique_ptr<BaseOperation>> operations;
    metrics::Operation metrics;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::pool::entry& client, ActorId id)
        : collection{(
              *client)[getDbName(phaseContext)][phaseContext["Collection"].to<std::string>()]},
          metrics{phaseContext.actor().operation("Crud", id)} {
        auto addOperation = [&](const Node& node) -> std::unique_ptr<BaseOperation> {
            auto& yamlCommand = node["OperationCommand"];
            auto opName = node["OperationName"].to<std::string>();
            auto onSession = yamlCommand["OnSession"].maybe<bool>().value_or(false);

            // Grab the appropriate Operation struct defined by 'OperationName'.
            auto op = opConstructors.find(opName);
            if (op == opConstructors.end()) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "Operation '" + opName + "' not supported in Crud Actor."));
            }

            // If the yaml specified MetricsName in the Phase block, then associate the metrics
            // with the Phase; otherwise, associate with the Actor (e.g. all bulkWrite operations
            // get recorded together across all Phases). The latter case (not specifying
            // MetricsName) is legacy configuration-syntax.
            //
            // Node is convertible to bool but only explicitly so need to do the odd-looking
            // `? true : false` thing.
            const bool perPhaseMetrics = phaseContext["MetricsName"] ? true : false;

            auto createOperation = op->second;
            return createOperation(yamlCommand,
                                   onSession,
                                   collection,
                                   perPhaseMetrics ? phaseContext.operation(opName, id)
                                                   : phaseContext.actor().operation(opName, id),
                                   phaseContext,
                                   id);
        };

        operations = phaseContext.getPlural<std::unique_ptr<BaseOperation>>(
            "Operation", "Operations", addOperation);
    }
};

void CrudActor::run() {
    for (auto&& config : _loop) {
        auto session = _client->start_session();
        for (const auto&& _ : config) {
            auto metricsContext = config->metrics.start();

            for (auto&& op : config->operations) {
                op->run(session);
            }

            metricsContext.success();
        }
    }
}

CrudActor::CrudActor(genny::ActorContext& context)
    : Actor(context),
      _client{std::move(
          context.client(context.get("ClientName").maybe<std::string>().value_or("Default")))},
      _loop{context, _client, CrudActor::id()} {}

namespace {
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}
}  // namespace genny::actor
