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
#include <cast_core/actors/OptionsConversion.hpp>
#include <cast_core/helpers/pipeline_helpers.hpp>

#include <chrono>
#include <memory>
#include <utility>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/json.hpp>
#include <bsoncxx/string/to_string.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>
#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>
#include <value_generators/PipelineGenerator.hpp>

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;
using bsoncxx::type;

namespace {

using namespace genny;

enum class ThrowMode {
    kSwallow,
    kRethrow,
    kSwallowAndRecord,  // Record failed operations but don't throw.
};

ThrowMode decodeThrowMode(const Node& operation, PhaseContext& phaseContext) {
    static const char* throwKey = "ThrowOnFailure";
    static const char* recordKey = "RecordFailure";

    bool throwOnFailure = operation[throwKey] ? operation[throwKey].to<bool>()
                                              : phaseContext[throwKey].maybe<bool>().value_or(true);
    bool recordFailure = operation[recordKey]
        ? operation[recordKey].to<bool>()
        : phaseContext[recordKey].maybe<bool>().value_or(false);
    if (recordFailure) {
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
                ctx.addErrors(1);

                // Record the failure but don't throw.
                ctx.failure();
                return;
            }
        }
        ctx.success();
    }

    template <class Model, class Options>
    void setUpsert(Model& model, Options options) {
        if (options.upsert()) {
            model.upsert(options.upsert().value());
        }
    }

    template <class Model, class Options>
    void setArrayFilters(Model& model, Options options) {
        if (options.array_filters()) {
            model.array_filters(options.array_filters().value());
        }
    }

    template <class Model, class Options>
    void setCollation(Model& model, Options options) {
        if (options.collation()) {
            model.collation(options.collation().value());
        }
    }

    template <class Model, class Options>
    void setHint(Model& model, Options options) {
        if (options.hint()) {
            model.hint(options.hint().value());
        }
    }

    virtual void run(mongocxx::client_session& session) = 0;
    virtual ~BaseOperation() = default;
};

class CollectionHandle {
public:
    CollectionHandle(mongocxx::client* client, std::string database, std::string collection)
        : _client(client), _database(std::move(database)), _collection(std::move(collection)) {}

    mongocxx::collection collection() {
        return (*_client)[_database][_collection];
    }

    mongocxx::client* client() {
        return _client;
    }

    const std::string& databaseName() const {
        return _database;
    }

    const std::string& collectionName() const {
        return _collection;
    }

private:
    mongocxx::client* _client;
    std::string _database;
    std::string _collection;
};

using OpCallback = std::function<std::unique_ptr<BaseOperation>(
    const Node&, bool, CollectionHandle, metrics::Operation, PhaseContext& context, ActorId id)>;

std::unordered_map<std::string, OpCallback&> getOpConstructors();

struct WriteOperation : public BaseOperation {
    WriteOperation(PhaseContext& phaseContext, const Node& operation)
        : BaseOperation(phaseContext, operation) {}
    virtual mongocxx::model::write getModel() = 0;
};

using WriteOpCallback = std::function<std::unique_ptr<WriteOperation>(
    const Node&, bool, CollectionHandle, metrics::Operation, PhaseContext&, ActorId)>;

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
                ? _collection.create_index(
                      session, keys.view(), indexOptions.view(), _operationOptions)
                : _collection.create_index(keys.view(), indexOptions.view(), _operationOptions);
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
        // Available options: http://mongocxx.org/api/current/classmongocxx_1_1model_1_1insert__one.html
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
          _update{opNode["Update"].to<DocumentGenerator>(context, id)},
          _options{opNode["OperationOptions"].maybe<mongocxx::options::update>().value_or(
              mongocxx::options::update{})} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        auto update = _update();
        mongocxx::model::update_one op{std::move(filter), std::move(update)};
        // Available options: http://mongocxx.org/api/current/classmongocxx_1_1model_1_1update__one.html
        setArrayFilters(op, _options);
        setCollation(op, _options);
        setHint(op, _options);
        setUpsert(op, _options);
        return op;
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
          _update{opNode["Update"].to<DocumentGenerator>(context, id)},
          _options{opNode["OperationOptions"].maybe<mongocxx::options::update>().value_or(
              mongocxx::options::update{})} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        auto update = _update();
        mongocxx::model::update_many op{std::move(filter), std::move(update)};
        // Available options: http://mongocxx.org/api/current/classmongocxx_1_1model_1_1update__many.html
        setArrayFilters(op, _options);
        setCollation(op, _options);
        setHint(op, _options);
        setUpsert(op, _options);
        return op;
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
          _filter(opNode["Filter"].to<DocumentGenerator>(context, id)),
          _options{opNode["OperationOptions"].maybe<mongocxx::options::delete_options>().value_or(
              mongocxx::options::delete_options{})} {}

    mongocxx::model::write getModel() override {
        auto filter = _filter();
        mongocxx::model::delete_one op{std::move(filter)};
        // Available options: http://mongocxx.org/api/current/classmongocxx_1_1model_1_1delete__one.html
        setCollation(op, _options);
        setHint(op, _options);
        return op;
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
        mongocxx::model::delete_many op{std::move(filter)};
        // Available options: http://mongocxx.org/api/current/classmongocxx_1_1model_1_1delete__many.html
        setCollation(op, _options);
        setHint(op, _options);
        return op;
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
        mongocxx::model::replace_one op{std::move(filter), std::move(replacement)};
        // Available options: http://mongocxx.org/api/current/classmongocxx_1_1model_1_1replace__one.html
        setCollation(op, _options);
        setHint(op, _options);
        setUpsert(op, _options);
        return op;
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
                    CollectionHandle collection,
                    metrics::Operation operation,
                    PhaseContext& context,
                    ActorId id) -> std::unique_ptr<P> {
    return std::make_unique<O>(opNode, onSession, collection.collection(), operation, context, id);
};

template <class P, class C, class O>
C collectionHandleCallback = [](const Node& opNode,
                                bool onSession,
                                CollectionHandle collection,
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
                       CollectionHandle collectionHandle,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collectionHandle{std::move(collectionHandle)},
          _collection{_collectionHandle.collection()},
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
        auto& yamlCommand = writeOp["OperationCommand"];
        bool onSession = yamlCommand["OnSession"].maybe<bool>().value_or(_onSession);
        _writeOps.push_back(
            createWriteOp(writeOp, onSession, _collectionHandle, _operation, context, id));
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
    CollectionHandle _collectionHandle;
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
        if (const auto& options = opNode["Options"]; options) {
            _options = options.to<mongocxx::options::find>();
            if (options["Projection"]) {
                _projection.emplace(options["Projection"].to<DocumentGenerator>(context, id));
            }
            if (options["Let"]) {
                _let.emplace(options["Let"].to<DocumentGenerator>(context, id));
            }
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        if (_projection) {
            _options.projection(_projection.value()());
        }
        if (_let) {
            _options.let(_let.value()());
        }
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
    std::optional<DocumentGenerator> _projection;
    std::optional<DocumentGenerator> _let;
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
        if (const auto& options = opNode["Options"]; options) {
            _options = options.to<mongocxx::options::find>();
            if (options["Projection"]) {
                _projection.emplace(options["Projection"].to<DocumentGenerator>(context, id));
            }
            if (options["Let"]) {
                _let.emplace(options["Let"].to<DocumentGenerator>(context, id));
            }
            if (options["Limit"]) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("Cannot specify 'limit' to 'findOne' operation"));
            }
            if (options["BatchSize"]) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "Cannot specify 'batchSize' to 'findOne' operation"));
            }
        }
    }

    void run(mongocxx::client_session& session) override {
        auto filter = _filter();
        if (_projection) {
            _options.projection(_projection.value()());
        }
        if (_let) {
            _options.let(_let.value()());
        }
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
    std::optional<DocumentGenerator> _projection;
    std::optional<DocumentGenerator> _let;
    metrics::Operation _operation;
};

struct AggregateOperation : public BaseOperation {
    AggregateOperation(const Node& opNode,
                       bool onSession,
                       mongocxx::collection collection,
                       metrics::Operation operation,
                       PhaseContext& context,
                       ActorId id)
        : BaseOperation(context, opNode),
          _onSession{onSession},
          _collection{std::move(collection)},
          _operation{operation},
          _pipelineGenerator{opNode["Pipeline"].to<PipelineGenerator>(context, id)} {
        if (const auto& options = opNode["Options"]; options) {
            _options = opNode["Options"].to<mongocxx::options::aggregate>();
            if (options["Let"]) {
                _let.emplace(options["Let"].to<DocumentGenerator>(context, id));
            }
        }
    }

    void run(mongocxx::client_session& session) override {
        auto pipeline = pipeline_helpers::makePipeline(_pipelineGenerator);
        if (_let) {
            _options.let(_let.value()());
        }
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            auto cursor = _onSession ? _collection.aggregate(session, pipeline, _options)
                                     : _collection.aggregate(pipeline, _options);
            for (auto&& doc : cursor) {
                ctx.addDocuments(1);
                ctx.addBytes(doc.length());
            }
            return pipeline_helpers::copyPipelineToDocument(pipeline);
        });
    }

private:
    bool _onSession;
    mongocxx::collection _collection;
    mongocxx::options::aggregate _options;
    std::optional<DocumentGenerator> _let;
    PipelineGenerator _pipelineGenerator;
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
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::insert>();
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
 * Warning: Commands that should be apart of the transaction must declare
 * the following value under OperationCommand: `OnSession: true`
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
                              [[maybe_unused]] mongocxx::collection collection,
                              metrics::Operation operation,
                              PhaseContext& context,
                              ActorId id)
        : BaseOperation(context, opNode), _operation{operation} {
        if (!onSession) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("'startTransaction' expects 'OnSession: true'."));
        };
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
                               [[maybe_unused]] mongocxx::collection collection,
                               metrics::Operation operation,
                               PhaseContext& context,
                               ActorId id)
        : BaseOperation(context, opNode), _operation{operation} {
        if (!onSession) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("'commitTransaction' expects 'OnSession: true'."));
        };
    }

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
 *    - OperationName: withTransaction
 *      OperationCommand:
 *        OnSession: true
 *        OperationsInTransaction:
 *          - OperationName: insertOne
 *            OperationCommand:
 *              Document: {a: 100}
 *          - OperationName: findOneAndReplace
 *            OperationCommand:
 *              Filter: {a: 100}
 *              Replacement: {a: 30}
 */

struct WithTransactionOperation : public BaseOperation {
    WithTransactionOperation(const Node& opNode,
                             bool onSession,
                             CollectionHandle collectionHandle,
                             metrics::Operation operation,
                             PhaseContext& context,
                             ActorId id)
        : BaseOperation(context, opNode),
          _collectionHandle{std::move(collectionHandle)},
          _operation{operation} {
        if (!onSession) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("'withTransaction' expects 'OnSession: true'."));
        };
        _onSession = onSession;
        auto& opsInTxn = opNode["OperationsInTransaction"];
        if (!opsInTxn.isSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'withTransaction' requires an 'OperationsInTransaction' list of operations."));
        }
        for (const auto&& [k, txnOp] : opsInTxn) {
            createTxnOps(txnOp, context, id);
        }
        if (opNode["Options"]) {
            _options = opNode["Options"].to<mongocxx::options::transaction>();
        }
    }

    void run(mongocxx::client_session& session) override {
        auto run_txn_ops([&](mongocxx::client_session* session) {
            for (auto&& op : _txnOps) {
                op->run(*session);
            }
        });
        this->doBlock(_operation, [&](metrics::OperationContext& ctx) {
            session.with_transaction(run_txn_ops, _options);
            return std::nullopt;
        });
    }

private:
    void createTxnOps(const Node& txnOp, PhaseContext& context, ActorId id) {
        auto opName = txnOp["OperationName"].to<std::string>();
        // MongoDB does not support transactions within transactions.
        std::set<std::string> excludeSet = {
            "startTransaction", "commitTransaction", "withTransaction"};
        if (excludeSet.find(opName) != excludeSet.end()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "Operation '" + opName + "' not supported inside a 'with_transaction' operation."));
        }

        std::string database =
            txnOp["Database"].maybe<std::string>().value_or(_collectionHandle.databaseName());
        std::string collection =
            txnOp["Collection"].maybe<std::string>().value_or(_collectionHandle.collectionName());
        CollectionHandle opCollectionHandle(
            _collectionHandle.client(), std::move(database), std::move(collection));

        auto opConstructors = getOpConstructors();
        auto opConstructor = opConstructors.find(opName);
        if (opConstructor == opConstructors.end()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "Operation '" + opName + "' not supported inside a 'with_transaction' operation."));
        }
        auto createOp = opConstructor->second;
        auto& yamlCommand = txnOp["OperationCommand"];
        // operations can override withTransaction's OnSession value
        // This behavior is hard to test.
        // The CrudActorYamlTests suite exercises both branches of this boolean
        // but doesn't assert the behavior.
        // Be careful when making changes around this code.
        bool onSession = yamlCommand["OnSession"].maybe<bool>().value_or(_onSession);
        _txnOps.push_back(createOp(
            yamlCommand, onSession, std::move(opCollectionHandle), _operation, context, id));
    }

    CollectionHandle _collectionHandle;
    bool _onSession;
    metrics::Operation _operation;
    mongocxx::options::transaction _options;
    std::vector<std::unique_ptr<BaseOperation>> _txnOps;
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

std::unordered_map<std::string, OpCallback&> getOpConstructors() {
    // Maps the yaml 'OperationName' string to the appropriate constructor of 'BaseOperation' type.
    std::unordered_map<std::string, OpCallback&> opConstructors = {
        {"bulkWrite", collectionHandleCallback<BaseOperation, OpCallback, BulkWriteOperation>},
        {"withTransaction",
         collectionHandleCallback<BaseOperation, OpCallback, WithTransactionOperation>},
        {"startTransaction", baseCallback<BaseOperation, OpCallback, StartTransactionOperation>},
        {"commitTransaction", baseCallback<BaseOperation, OpCallback, CommitTransactionOperation>},
        {"aggregate", baseCallback<BaseOperation, OpCallback, AggregateOperation>},
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
        {"setReadConcern", baseCallback<BaseOperation, OpCallback, SetReadConcernOperation>},
        {"drop", baseCallback<BaseOperation, OpCallback, DropOperation>},
        {"insertOne", baseCallback<BaseOperation, OpCallback, InsertOneOperation>},
        {"deleteOne", baseCallback<BaseOperation, OpCallback, DeleteOneOperation>},
        {"deleteMany", baseCallback<BaseOperation, OpCallback, DeleteManyOperation>},
        {"updateOne", baseCallback<BaseOperation, OpCallback, UpdateOneOperation>},
        {"updateMany", baseCallback<BaseOperation, OpCallback, UpdateManyOperation>},
        {"replaceOne", baseCallback<BaseOperation, OpCallback, ReplaceOneOperation>}};

    return opConstructors;
}

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

enum class Units { kNanosecond, kMicrosecond, kMillisecond, kSecond, kMinute, kHour };
// represent a random delay
class Delay {
public:
    Delay(const Node& node, GeneratorArgs args)
        : numberGenerator{makeDoubleGenerator(node["^TimeSpec"]["value"], args)} {
        if (!node["^TimeSpec"]["unit"]) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("Each TimeSpec needs a unit declaration."));
        }
        auto unitString = node["^TimeSpec"]["unit"].to<std::string>();

        // Use string::find here so plurals get parsed correctly.
        if (unitString.find("nanosecond") == 0) {
            units = Units::kNanosecond;
        } else if (unitString.find("microsecond") == 0) {
            units = Units::kMicrosecond;
        } else if (unitString.find("millisecond") == 0) {
            units = Units::kMillisecond;
        } else if (unitString.find("second") == 0) {
            units = Units::kSecond;
        } else if (unitString.find("minute") == 0) {
            units = Units::kMinute;
        } else if (unitString.find("hour") == 0) {
            units = Units::kHour;
        } else {
            std::stringstream msg;
            msg << "Invalid unit: " << unitString << " for genny::TimeSpec field in config";
            throw genny::InvalidConfigurationException(msg.str());
        }
    }

    // Evaluate the generator to produce a random delay.
    Duration evaluate() {
        auto value = numberGenerator.evaluate();
        switch (units) {
            case Units::kNanosecond:
                return (std::chrono::duration_cast<Duration>(
                    std::chrono::duration<double, std::nano>(value)));
                break;
            case Units::kMicrosecond:
                return (std::chrono::duration_cast<Duration>(
                    std::chrono::duration<double, std::nano>(value)));
                break;
            case Units::kMillisecond:
                return (std::chrono::duration_cast<Duration>(
                    std::chrono::duration<double, std::milli>(value)));
                break;
            case Units::kSecond:
                return (std::chrono::duration_cast<Duration>(std::chrono::duration<double>(value)));
                break;
            case Units::kMinute:
                return (std::chrono::duration_cast<Duration>(
                    std::chrono::duration<double, std::ratio<60>>(value)));
                break;
            case Units::kHour:
                return (std::chrono::duration_cast<Duration>(
                    std::chrono::duration<double, std::ratio<3600>>(value)));
                break;
        }
    }

private:
    TypeGenerator<double> numberGenerator;
    Units units;
};

struct Transition {
    int nextState;
    Delay delay;
};

// Represents one state in an FSM.
class State {
public:
    std::vector<std::unique_ptr<BaseOperation>> operations;
    std::string stateName;
    std::vector<double> transitionWeights;
    std::vector<Transition> transitions;

    template <typename A>
    State(const Node& node,
          const std::unordered_map<std::string, int>& states,
          PhaseContext& phaseContext,
          ActorId id,
          A&& addOperation)
        : operations{node.getPlural<std::unique_ptr<BaseOperation>>(
              "Operation", "Operations", addOperation)},
          stateName{node["Name"].to<std::string>()} {
        for (const auto&& [k, transitionYaml] : node["Transitions"]) {
            if (!transitionYaml["Weight"] || !transitionYaml["Weight"].isScalar()) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("Each transition must have a scalar weight"));
            }
            if (!transitionYaml["To"] || !transitionYaml["To"].isScalar()) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("Each transition must have a scalar 'To' entry"));
            }
            transitionWeights.emplace_back(transitionYaml["Weight"].template to<double>());
            transitions.emplace_back(Transition{
                states.at(transitionYaml["To"].template to<std::string>()),
                Delay(transitionYaml["SleepBefore"], GeneratorArgs{phaseContext.rng(id), id})});
        }
    }
};

// Helper struct to encapsulate state based behavior
struct StateMachine {
    PhaseContext& phaseContext;

    std::vector<State> states;
    std::vector<double> initialStateWeights;

    bool continueCurrentState;
    bool skipFirstOperations;
    bool skip;

    int currentState;
    int nextState;

    ActorId actorId;

    explicit StateMachine(PhaseContext& phaseContext, ActorId actorId)
        : phaseContext{phaseContext},
          states{},
          initialStateWeights{},
          continueCurrentState{phaseContext["Continue"].maybe<bool>().value_or(false)},
          skipFirstOperations{phaseContext["SkipFirstOperations"].maybe<bool>().value_or(false)},
          skip{false},
          currentState{0},
          nextState{0},
          actorId{actorId} {}

    template <typename F>
    void setup(F&& addOperation) {
        // Check if we have Operations or States. Throw an error if we have both.
        if ((phaseContext["Operations"] || phaseContext["Operation"]) && phaseContext["States"]) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "CrudActor has Operations and States at the same time."));
        }

        if (!phaseContext["States"]) {
            return;
        }

        int i = 0;
        std::unordered_map<std::string, int> stateNames;
        for (auto [k, state] : phaseContext["States"]) {
            if (!state["Name"] || !state["Name"].isScalar()) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "Each state must have a name and it must be a string"));
            }
            stateNames.emplace(state["Name"].template to<std::string>(), i);
            i++;
        }

        auto numStates = stateNames.size();

        if (continueCurrentState) {
            // Check that this isn't phase 0
            auto myPhaseNumber = phaseContext.getPhaseNumber();
            if (myPhaseNumber == 0) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "CrudActor has continue set for Phase 0. Nothing to continue from."));
            }
            // TODO: check that the previous state has <= number of states as this one
            // Need to access the previous phase config.

        } else if (!phaseContext["InitialStates"]) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "CrudActor has States, but has not specified InitialStates"));
        }

        states.reserve(numStates);
        for (auto [k, stateNode] : phaseContext["States"]) {
            states.emplace_back<State>(
                {stateNode, stateNames, phaseContext, actorId, addOperation});
        }

        initialStateWeights = std::vector<double>(numStates, 0);
        for (auto [k, state] : phaseContext["InitialStates"]) {
            initialStateWeights[stateNames[state["State"].template to<std::string>()]] +=
                state["Weight"].template to<double>();
        }
    }

    auto& operations() {
        return states[currentState].operations;
    }

    auto transitionDelay(int transition) {
        return states[currentState].transitions[transition].delay.evaluate();
    }

    template <typename S>
    std::optional<Duration> run(S& session, DefaultRandom& rng) {
        if (!active()) {
            return std::nullopt;
        }
        // Simplifying Assumption -- one shot operations on state enter
        // We are here to fire those operations, and then pick the next state.
        // transition to the next state
        currentState = nextState;

        BOOST_LOG_TRIVIAL(debug) << "Actor " << actorId << " running in state " << currentState;


        // run the operations for the next state  unless  we have set to skip the first set
        // of operations
        if (!skip) {
            BOOST_LOG_TRIVIAL(debug)
                << "Actor " << actorId << " running operations for state " << currentState;
            for (auto&& op : operations()) {
                op->run(session);
            }
        } else {
            skip = false;
        }

        auto transition = pickNext(rng);
        auto delay = transitionDelay(transition);

        return {delay};
    }

    [[nodiscard]] bool active() const {
        return !states.empty();
    }

    [[nodiscard]] int pickNext(DefaultRandom& rng) {
        auto distribution =
            boost::random::discrete_distribution(states[currentState].transitionWeights);
        int transition = distribution(rng);
        nextState = states[currentState].transitions[transition].nextState;
        return transition;
    }

    void onNewPhase(bool nop, DefaultRandom& rng) {
        skip = false;
        // TODO: If running without states, define a single state with the operations
        if (!nop && !states.empty()) {
            if (!continueCurrentState) {
                // pick the initial state for the phase
                auto initial_distribution =
                    boost::random::discrete_distribution(initialStateWeights);
                nextState = initial_distribution(rng);
                BOOST_LOG_TRIVIAL(debug)
                    << "Actor " << actorId << " picking initial state " << nextState;
            } else {
                // continue from the previous phase
                nextState = currentState;
                BOOST_LOG_TRIVIAL(debug)
                    << "Actor " << actorId << " continuing from previous state " << nextState;
            }
            if (skipFirstOperations) {
                skip = true;
            }
        }
    }
};


}  // namespace

namespace genny::actor {

struct CrudActor::CollectionName {
    std::optional<std::string> collectionName;
    std::optional<int64_t> numCollections;
    explicit CollectionName(PhaseContext& phaseContext)
        : collectionName{phaseContext["Collection"].maybe<std::string>()},
          numCollections{phaseContext["CollectionCount"].maybe<IntegerSpec>()} {
        if (collectionName && numCollections) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "Collection or CollectionCount, not both in Crud Actor."));
        }
        if (!collectionName && !numCollections) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "One of Collection or CollectionCount must be provided in Crud Actor."));
        }
    }
    // Get the assigned collection name or generate a name based on collectionCount and the
    // the actorId.
    std::string generateName(ActorId id) {
        if (collectionName) {
            return collectionName.value();
        }
        auto collectionNumber = id % numCollections.value();
        return "Collection" + std::to_string(collectionNumber);
    }
};
struct CrudActor::PhaseConfig {
    std::vector<std::unique_ptr<BaseOperation>> operations;
    metrics::Operation metrics;
    StateMachine fsm;
    std::string dbName;
    CrudActor::CollectionName collectionName;

    std::unique_ptr<BaseOperation> addOperation(const Node& node,
                                                mongocxx::pool::entry& client,
                                                const std::string& name,
                                                PhaseContext& phaseContext,
                                                ActorId id) const {
        std::string database = node["Database"].maybe<std::string>().value_or(dbName);
        std::string collection = node["Collection"].maybe<std::string>().value_or(name);
        CollectionHandle collectionHandle(&*client, std::move(database), std::move(collection));

        auto& yamlCommand = node["OperationCommand"];
        auto opName = node["OperationName"].to<std::string>();
        auto opMetricsName = node["OperationMetricsName"].maybe<std::string>().value_or(opName);
        std::set<std::string> transactionOps = {
            "startTransaction", "commitTransaction", "withTransaction"};
        bool isTransactionOp = transactionOps.find(opName) != transactionOps.end();
        auto onSession = yamlCommand["OnSession"].maybe<bool>().value_or(isTransactionOp);

        auto opConstructors = getOpConstructors();
        // Grab the appropriate Operation struct defined by 'OperationName'.
        auto op = opConstructors.find(opName);
        if (op == opConstructors.end()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("Operation '" + opName +
                                                                "' not supported in Crud Actor."));
        }

        /*
        The code section below is equivalent to the following pseudocode (because
        `metrics::Operation` doesn't currently allow declaration without initialization):

        metrics::Operation opMetrics;
        auto metricsName = phaseContext["MetricsName"].maybe<std::string>();
        auto opMetricsName = node["OperationMetricsName"].maybe<std::string>();
        if (metricsName && opMetricsName) {
            opMetrics = phaseContext.actor().operation(*metricsName + "." + *opMetricsName, id);
        } else if (metricsName) {
            opMetrics = phaseContext.operation(opName, id);
        }
        else if (opMetricsName) {
            opMetrics = phaseContext.actor().operation(*opMetricsName, id);
        } else {
            opMetrics = phaseContext.actor().operation(opName, id);
        }
        auto opCreator = op->second;
        return opCreator(yamlCommand, onSession, std::move(collectionHandle), opMetrics,
                         phaseContext, id);
        */

        // If the yaml specified MetricsName in the Phase block, then associate the metrics
        // with the Phase; otherwise, associate with the Actor (e.g. all bulkWrite
        // operations get recorded together across all Phases). The latter case (not
        // specifying MetricsName) is legacy configuration-syntax.
        //
        // Node is convertible to bool but only explicitly.
        const bool perPhaseMetrics = bool(phaseContext["MetricsName"]);
        auto opCreator = op->second;

        if (auto metricsName = phaseContext["MetricsName"].maybe<std::string>();
            metricsName && opMetricsName != opName) {
            // Special case if we have set MetricsName at the phase level, and OperationMetricsName
            // on the operation. Ideally the operation constructors would directly support this
            // functionality, and be common to all actors. At the time this was added, it was
            // non-trivial to do so in a non-breaking fashion.
            std::ostringstream stm;
            stm << *metricsName << "." << opMetricsName;

            return opCreator(yamlCommand,
                             onSession,
                             std::move(collectionHandle),
                             phaseContext.actor().operation(stm.str(), id),
                             phaseContext,
                             id);
        }

        return opCreator(yamlCommand,
                         onSession,
                         std::move(collectionHandle),
                         perPhaseMetrics ? phaseContext.operation(opMetricsName, id)
                                         : phaseContext.actor().operation(opMetricsName, id),
                         phaseContext,
                         id);
    }

    PhaseConfig(PhaseContext& phaseContext, mongocxx::pool::entry& client, ActorId id)
        : dbName{getDbName(phaseContext)},
          metrics{phaseContext.actor().operation("Crud", id)},
          fsm{phaseContext, id},
          collectionName{phaseContext} {

        auto name = collectionName.generateName(id);
        auto addOpCallback = [&](const Node& node) -> std::unique_ptr<BaseOperation> {
            return addOperation(node, client, name, phaseContext, id);
        };
        fsm.setup(addOpCallback);

        if (phaseContext["Operations"] || phaseContext["Operation"]) {
            if (phaseContext["Continue"]) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("Continue option not valid if not using States"));
            }
            if (phaseContext["SkipFirstOperations"]) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "SkipFirstOperations option not valid if not using States"));
            }
            operations = phaseContext.getPlural<std::unique_ptr<BaseOperation>>(
                "Operation", "Operations", addOpCallback);
        } else if (!phaseContext["States"]) {  // Throw a useful error
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("CrudActor has neither Operations nor States "
                                              "specified. Exactly one must be defined."));
        }
    }
};

void CrudActor::run() {
    for (auto&& config : _loop) {
        auto session = _client->start_session();
        if (!config.isNop()) {
            config->fsm.onNewPhase(config.isNop(), _rng);
        }
        for (const auto&& _ : config) {
            auto metricsContext = config->metrics.start();
            // std::optionals (like `delay`) convert to boolean true iff their value is present.
            if (auto delay = config->fsm.run(session, _rng); delay) {
                config.sleepNonBlocking(*delay);
            } else {
                BOOST_LOG_TRIVIAL(debug) << "Actor " << id() << " running regular (non-fsm)";
                for (auto&& op : config->operations) {
                    op->run(session);
                }
            }
            metricsContext.success();
        }
    }
}

CrudActor::CrudActor(genny::ActorContext& context)
    : Actor(context),
      _client{std::move(context.client())},
      _loop{context, _client, CrudActor::id()},
      _rng{context.workload().getRNGForThread(CrudActor::id())} {}

namespace {
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}
}  // namespace genny::actor
