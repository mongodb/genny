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

#include <cast_core/actors/MatViewActor.hpp>
#include <cast_core/actors/OptionsConversion.hpp>

#include <chrono>
#include <memory>
#include <utility>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/exception/query_exception.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/string/to_string.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>
#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

#include <random>
#include <string>

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <cstdint>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <vector>


using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

namespace genny::actor {

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

std::string makeOperationName(std::string experimentType,
                              bool isTransactional,
                              std::string numGroupsAndDistribution,
                              int64_t insertCount,
                              bool isInsertMany,
                              int64_t numMatViews,
                              std::string matViewMaintenanceMode) {
    std::ostringstream stm;
    stm << "MV.";
    if (experimentType == "WildCardIndexExperiment") {
        stm << "WildCardIndex.";
        stm << (isTransactional ? "xact" : "nonxact") << ".";
        stm << insertCount << "inserts.";
        stm << (isInsertMany ? "insertmany" : "insertone") << ".";
    } else {
        stm << "MatView.";
        stm << (isTransactional ? "xact" : "nonxact") << ".";
        stm << insertCount << "inserts.";
        stm << (isInsertMany ? "insertmany" : "insertone") << ".";
        stm << numMatViews << "views.";
        stm << numGroupsAndDistribution << ".";
        stm << matViewMaintenanceMode;
    }
    return stm.str();
};

struct ExperimentSetting {
    explicit ExperimentSetting(PhaseContext& context,
                               ActorId id,
                               std::string experimentType,
                               bool isTransactional,
                               std::string numGroupsAndDistribution,
                               int64_t insertCount,
                               bool isInsertMany,
                               int64_t numMatViews,
                               std::string matViewMaintenanceMode)
        : _experimentType(experimentType),
          _isTransactional(isTransactional),
          _numGroupsAndDistribution(numGroupsAndDistribution),
          _insertCount(insertCount),
          _isInsertMany(isInsertMany),
          _numMatViews(numMatViews),
          _matViewMaintenanceMode(matViewMaintenanceMode),
          _operation(context.operation(makeOperationName(experimentType,
                                                         isTransactional,
                                                         numGroupsAndDistribution,
                                                         insertCount,
                                                         isInsertMany,
                                                         numMatViews,
                                                         matViewMaintenanceMode),
                                       id)) {}

    ~ExperimentSetting() = default;

    std::string _experimentType;
    bool _isTransactional;
    std::string _numGroupsAndDistribution;
    int64_t _insertCount;
    bool _isInsertMany;
    int64_t _numMatViews;
    std::string _matViewMaintenanceMode;

    metrics::Operation _operation;
};

/**
 * Example:
 * ```yaml

 * Actors:
 * - Name: UpdateDocumentsInTransactionActor
 *   Type: MatViewActor
 *   Database: &db test
 *   Threads: 32
 *   Phases:
 *   - MetricsName: MaintainView
 *     Repeat: *numInsertBatchesPerClinet
 *     Database: *db
 *     Collection: Collection0
 *     Operations:
 *     - OperationName: matView
 *         OperationCommand:
 *           Debug: false
 *           Database: *db
 *           ThrowOnFailure: false
 *           RecordFailure: true
 *           InsertDocument:
 *             k: {^Inc: {start: 0}}
 *           TransactionOptions:
 *             MaxCommitTime: 500 milliseconds
 *             WriteConcern:
 *               Level: majority
 *               Journal: true
 *             ReadConcern:
 *               Level: snapshot
 *             ReadPreference:
 *               ReadMode: primaryPreferred
 *               MaxStaleness: 1000 seconds
 * ```
 *
 * Owner: Query
 */
struct MatViewOperation : public BaseOperation {
    MatViewOperation(const Node& opNode,
                     mongocxx::pool::entry& client,
                     const std::string& dbName,
                     const std::string& collectionName,
                     PhaseContext& context,
                     ActorId id)
        : BaseOperation(context, opNode),
          _dbName{opNode["Database"].to<std::string>()},
          _collection{std::move((*client)[dbName][collectionName])},
          _insertDocumentExpr{opNode["InsertDocument"].to<DocumentGenerator>(context, id)} {
        if (opNode["TransactionOptions"]) {
            _transactionOptions = opNode["TransactionOptions"].to<mongocxx::options::transaction>();
        }
        if (opNode["Debug"]) {
            _isDebug = opNode["Debug"].to<std::string>() == "true";
        } else {
            std::cout << "opNode[Debug] does not exist!" << std::endl;
        }

        // Transactional: &isTransactional [true, false]
        // NumGroupsAndDistribution: &numGroupsAndDistribution [
        //     "uniform_1_1",
        //     "uniform_1_10",
        //     "uniform_1_100",
        //     "uniform_1_1000",
        //     "uniform_1_10000",
        //     "binomial",
        //     "geometric",
        // ]
        // NumInsertOpsPerClinetBatch: &numInsertOpsPerClinetBatch 100 # [ 100 ]
        // ClientBatchInsertMode: &clientBatchInsertMode ["insertOne", "insertMany"]
        // NumMatViews: &numMatViews [0, 1, 2, 4, 8]
        // MatViewMaintenanceMode: &matViewMaintenanceMode [ "sync-incremental" ]

        // std::vector<bool> isTransactionalOpts{true, false};
        // std::vector<std::string> numGroupsAndDistributionOpts{
        //     "uniform_1_1",
        //     "uniform_1_10",
        //     "uniform_1_100",
        //     "uniform_1_1000",
        //     "uniform_1_10000",
        //     "binomial",
        //     "geometric",
        // };
        // std::vector<int64_t> insertCountOpts{100};
        // std::vector<bool> isInsertManyOpts{false, true};
        // std::vector<int64_t> numMatViewsOpts{0, 1, 2, 4, 8};
        // std::vector<std::string> matViewMaintenanceModeOpts{
        //     "sync-incremental", "async-incremental-result-delta", "async-incremental-base-delta",
        //     /*, "full-refresh"*/};


        std::vector<bool> isTransactionalOpts{false};
        std::vector<std::string> numGroupsAndDistributionOpts{
            "uniform_1_10",
        };
        std::vector<int64_t> insertCountOpts{100};
        std::vector<bool> isInsertManyOpts{true};
        std::vector<int64_t> numMatViewsOpts{2};
        std::vector<std::string> matViewMaintenanceModeOpts{"async-incremental-result-delta"};


        for (const auto isTransactional : isTransactionalOpts) {
            for (const auto insertCount : insertCountOpts) {
                for (const auto isInsertMany : isInsertManyOpts) {
                    for (const auto numMatViews : numMatViewsOpts) {
                        if (numMatViews > 0) {
                            for (const auto numGroupsAndDistribution :
                                 numGroupsAndDistributionOpts) {
                                for (const auto matViewMaintenanceMode :
                                     matViewMaintenanceModeOpts) {
                                    _experimentSettings.push_back(
                                        ExperimentSetting(context,
                                                          id,
                                                          "MatViewExperiment",
                                                          isTransactional,
                                                          numGroupsAndDistribution,
                                                          insertCount,
                                                          isInsertMany,
                                                          numMatViews,
                                                          matViewMaintenanceMode));
                                }
                            }
                        } else {
                            _experimentSettings.push_back(ExperimentSetting(context,
                                                                            id,
                                                                            "MatViewExperiment",
                                                                            isTransactional,
                                                                            "none",
                                                                            insertCount,
                                                                            isInsertMany,
                                                                            numMatViews,
                                                                            "none"));
                        }
                    }
                }
            }
        }

        // for (const auto isTransactional : isTransactionalOpts) {
        //     for (const auto insertCount : insertCountOpts) {
        //         for (const auto isInsertMany : isInsertManyOpts) {
        //             _experimentSettings.push_back(ExperimentSetting(context,
        //                                                             id,
        //                                                             "WildCardIndexExperiment",
        //                                                             isTransactional,
        //                                                             "none",
        //                                                             insertCount,
        //                                                             isInsertMany,
        //                                                             0,
        //                                                             "none"));
        //         }
        //     }
        // }
    }

    std::string generateRandStr(int max_length) {
        thread_local std::string possible_characters =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        thread_local std::random_device rd;
        thread_local std::mt19937 engine(rd());
        thread_local std::uniform_int_distribution<> dist(0, possible_characters.size() - 1);
        std::string ret = "";
        for (int i = 0; i < max_length; i++) {
            int random_index = dist(engine);  // get index between 0 and
                                              // possible_characters.size()-1
            ret += possible_characters[random_index];
        }
        return ret;
    }

    std::unordered_map<int, std::tuple<bsoncxx::types::b_oid, int>> combineInsertedDocs(
        const std::vector<bsoncxx::document::view_or_value>& insertedDocs,
        std::map<std::size_t, bsoncxx::types::b_oid> insertedIds,
        size_t matViewIdx,
        const std::string& numGroupsAndDistribution) {
        std::unordered_map<int, std::tuple<bsoncxx::types::b_oid, int>> combined;
        size_t i = 0;
        for (const auto& doc : insertedDocs) {
            auto y = doc.view()["y_" + numGroupsAndDistribution].get_int64();
            auto t = doc.view()["t" + std::to_string(matViewIdx)].get_int64();
            auto it = combined.find(y);
            if (it != combined.end()) {
                auto [_id, existing_t] = it->second;
                it->second = std::make_tuple(_id, existing_t + t);
            } else {
                combined[y] = std::make_tuple(insertedIds[i], t);
            }
            ++i;
        }
        return combined;
    }


    void run(mongocxx::client_session& runSession) override {
        using namespace bsoncxx::builder::basic;

        for (auto es : _experimentSettings) {
            std::string _experimentType = es._experimentType;
            bool _isTransactional = es._isTransactional;
            std::string _numGroupsAndDistribution = es._numGroupsAndDistribution;
            int64_t _insertCount = es._insertCount;
            bool _isInsertMany = es._isInsertMany;
            int64_t _numMatViews = es._numMatViews;
            std::string _matViewMaintenanceMode = es._matViewMaintenanceMode;

            static auto sampleViewDoc = make_document(kvp("_id", 0), kvp("t0_sum", 0));

            std::vector<bsoncxx::document::view_or_value> writeOps;
            size_t bytes = 0;
            for (size_t i = 0; i < _insertCount; ++i) {
                auto doc = _insertDocumentExpr();
                bytes += doc.view().length();
                writeOps.emplace_back(std::move(doc));
            }

            mongocxx::write_concern wc;
            wc.journal(true);
            wc.acknowledge_level(mongocxx::write_concern::level::k_majority);

            mongocxx::options::insert insertOptions;
            insertOptions.ordered(false);
            if (!_isTransactional) {
                insertOptions.write_concern(wc);
            }

            this->doBlock(es._operation, [&](metrics::OperationContext& ctx) {
                auto run_view_maintenance([&](mongocxx::client_session& session,
                                              const std::vector<bsoncxx::document::view_or_value>&
                                                  insertedDocs,
                                              std::map<std::size_t, bsoncxx::types::b_oid>
                                                  insertedIds) {
                    mongocxx::read_concern rc;
                    rc.acknowledge_level(mongocxx::read_concern::level::k_snapshot);

                    mongocxx::options::aggregate aggOptions;
                    if (!_isTransactional) {
                        aggOptions.read_concern(rc);
                        aggOptions.write_concern(wc);
                    }
                    aggOptions.allow_disk_use(true);

                    mongocxx::options::update updateOptions;
                    if (!_isTransactional) {
                        updateOptions.write_concern(wc);
                    }
                    updateOptions.upsert(true);

                    mongocxx::options::delete_options deleteOptions;
                    if (!_isTransactional) {
                        deleteOptions.write_concern(wc);
                    }

                    auto db = session.client().database(_dbName);
                    if (_matViewMaintenanceMode == "none") {
                        // nothing to do for the case of 0 mat-views
                    } else if (_matViewMaintenanceMode == "sync-incremental") {
                        if (_isDebug) {
                            std::cout << "Running sync-incremental view maintenance..."
                                      << std::endl;
                        }
                        for (size_t matViewIdx = 0; matViewIdx < _numMatViews; ++matViewIdx) {
                            std::string targetViewName = "Collection0MatView" +
                                std::to_string(matViewIdx) + "_" + _numGroupsAndDistribution;
                            auto combinedInsertedDocs = combineInsertedDocs(
                                insertedDocs, insertedIds, matViewIdx, _numGroupsAndDistribution);
                            for (const auto& combinedDoc : combinedInsertedDocs) {
                                auto yVal = combinedDoc.first;
                                auto [_, tVal] = combinedDoc.second;
                                const auto& updateFind = make_document(kvp("_id", yVal));
                                const auto& updateExpr = make_document(
                                    kvp("$inc",
                                        make_document(
                                            kvp("t" + std::to_string(matViewIdx) + "_sum", tVal))));
                                auto coll = db.collection(targetViewName);
                                auto updateRes = _isTransactional
                                    ? coll.update_one(session,
                                                      updateFind.view(),
                                                      updateExpr.view(),
                                                      updateOptions)
                                    : coll.update_one(
                                          updateFind.view(), updateExpr.view(), updateOptions);
                                if (!updateRes ||
                                    (updateRes->modified_count() != 1 &&
                                     !updateRes->upserted_id())) {
                                    // BOOST_THROW_EXCEPTION(
                                    //     std::runtime_error("Incremental MatView
                                    //     failded."));
                                    std::cout << "error: Incremental MatView failded." << std::endl;
                                } else {
                                    ctx.addDocuments(1);
                                    ctx.addBytes(sampleViewDoc.view().length());
                                }
                            }
                        }
                    } else if (_matViewMaintenanceMode == "async-incremental-result-delta") {
                        if (_isDebug) {
                            std::cout
                                << "Running async-incremental-result-delta view maintenance..."
                                << std::endl;
                        }
                        for (size_t matViewIdx = 0; matViewIdx < _numMatViews; ++matViewIdx) {
                            std::string targetViewDeltaName = "Collection0MatView" +
                                std::to_string(matViewIdx) + "_" + _numGroupsAndDistribution +
                                "_Delta";
                            auto combinedInsertedDocs = combineInsertedDocs(
                                insertedDocs, insertedIds, matViewIdx, _numGroupsAndDistribution);
                            std::vector<bsoncxx::document::view_or_value> deltaOutputDocs;
                            for (const auto& combinedDoc : combinedInsertedDocs) {
                                auto yVal = combinedDoc.first;
                                auto [idVal, tVal] = combinedDoc.second;
                                bsoncxx::document::view_or_value doc(
                                    bsoncxx::builder::stream::document{}
                                    << "_id" << idVal << "y_" + _numGroupsAndDistribution << yVal
                                    << "t" + std::to_string(matViewIdx) << tVal << finalize);
                                deltaOutputDocs.push_back(doc);
                                ctx.addDocuments(1);
                                ctx.addBytes(doc.view().length());
                            }
                            db.collection(targetViewDeltaName)
                                .insert_many(deltaOutputDocs, insertOptions);
                        }
                    } else if (_matViewMaintenanceMode == "async-incremental-base-delta") {
                        if (_isDebug) {
                            std::cout << "Running async-incremental-base-delta view maintenance..."
                                      << std::endl;
                        }
                        std::string targetBaseDeltaName = "Collection0_Delta";
                        db.collection(targetBaseDeltaName).insert_many(insertedDocs, insertOptions);
                    } else if (_matViewMaintenanceMode == "full-refresh") {
                        if (_isDebug) {
                            std::cout << "Running full-refresh view maintenance..." << std::endl;
                        }
                        for (size_t matViewIdx = 0; matViewIdx < _numMatViews; ++matViewIdx) {
                            std::string targetViewName = "Collection0MatView" +
                                std::to_string(matViewIdx) + "_" + _numGroupsAndDistribution;

                            mongocxx::pipeline p{};
                            p.group(make_document(
                                kvp("_id", "$y_" + _numGroupsAndDistribution),
                                kvp("t" + std::to_string(matViewIdx) + "_sum",
                                    make_document(
                                        kvp("$sum", "$t" + std::to_string(matViewIdx))))));
                            if (_isTransactional) {
                                std::string tempCollName = "tempColl" + generateRandStr(10);
                                auto aggOutRes = _collection.aggregate(session, p, aggOptions);


                                std::vector<bsoncxx::document::view_or_value> aggOutDocs;
                                for (auto doc : aggOutRes) {
                                    aggOutDocs.push_back(doc);
                                }
                                if (aggOutDocs.size() != 0L) {
                                    auto templColl = db.create_collection(tempCollName,
                                                                          make_document() /*, wc*/);
                                    auto insertRes =
                                        templColl.insert_many(aggOutDocs, insertOptions);
                                    if (!insertRes ||
                                        insertRes->inserted_count() != aggOutDocs.size()) {
                                        // BOOST_THROW_EXCEPTION(
                                        //     std::runtime_error("Inserting full-refresh results
                                        //     into "
                                        //                        "temp collection failed."));
                                        std::cout
                                            << "error: Inserting full-refresh results into temp "
                                               "collection failed."
                                            << std::endl;
                                    }

                                    ctx.addDocuments(aggOutDocs.size());
                                    ctx.addBytes(aggOutDocs.size() * sampleViewDoc.view().length());
                                    templColl.rename(targetViewName, true, wc);
                                } else {
                                    auto deleteManyRes =
                                        db.collection(targetViewName)
                                            .delete_many(session, make_document(), deleteOptions);
                                    if (!deleteManyRes) {
                                        // BOOST_THROW_EXCEPTION(
                                        //     std::runtime_error("Deleting existing documents from
                                        //     the
                                        //     "
                                        //                        "mat-view failed (in
                                        //                        full-refresh)."));
                                        std::cout << "error: Deleting existing documents from the "
                                                     "mat-view failed (in full-refresh)."
                                                  << std::endl;
                                    } else {
                                        ctx.addDocuments(deleteManyRes->deleted_count());
                                    }
                                }
                            } else {
                                p.merge(
                                    make_document(kvp("into", targetViewName), kvp("on", "_id")));

                                auto aggOutRes = _collection.aggregate(p, aggOptions);
                                // TODO: validate aggOutRes
                            }
                        }
                    }
                });
                auto run_txn_ops([&](mongocxx::client_session* session) {
                    if (_isInsertMany) {
                        if (_isDebug) {
                            std::cout << "InsertMany(insert_count = " << _insertCount
                                      << ", writeOps = " << writeOps.size()
                                      << ", _isTransactional = " << _isTransactional << ")"
                                      << std::endl;
                        }
                        auto result = (_isTransactional)
                            ? _collection.insert_many(*session, writeOps, insertOptions)
                            : _collection.insert_many(writeOps, insertOptions);
                        if (result) {
                            ctx.addDocuments(result->inserted_count());
                            std::map<std::size_t, bsoncxx::types::b_oid> insertedIds;
                            for (const auto insertedId : result->inserted_ids()) {
                                insertedIds[insertedId.first] = insertedId.second.get_oid();
                            }
                            run_view_maintenance(*session, writeOps, insertedIds);
                        } else {
                            // BOOST_THROW_EXCEPTION(std::runtime_error("InsertMany failded in
                            // MatView."));
                            std::cout << "error: InsertMany failded in MatView." << std::endl;
                        }
                    } else {
                        size_t i = 0;
                        for (auto&& document : writeOps) {
                            if (_isDebug) {
                                std::cout << (++i) << " - InsertOne(insert_count = " << _insertCount
                                          << ", writeOps = " << writeOps.size()
                                          << ", _isTransactional = " << _isTransactional << ")"
                                          << std::endl;
                            }
                            auto result = (_isTransactional)
                                ? _collection.insert_one(*session, document.view(), insertOptions)
                                : _collection.insert_one(document.view(), insertOptions);
                            if (result) {
                                ctx.addDocuments(1);
                                run_view_maintenance(*session,
                                                     {document},
                                                     std::map<std::size_t, bsoncxx::types::b_oid>{
                                                         {0, result->inserted_id().get_oid()}});
                            } else {
                                // BOOST_THROW_EXCEPTION(
                                //     std::runtime_error("InsertOne failded in MatView."));
                                std::cout << "error: InsertOne failded in MatView." << std::endl;
                            }
                        }
                    }
                    ctx.addBytes(bytes);
                });
                try {
                    if (_isTransactional) {
                        if (_isDebug) {
                            std::cout << "== Started run with transaction" << std::endl;
                        }
                        runSession.with_transaction(run_txn_ops, _transactionOptions);
                    } else {
                        if (_isDebug) {
                            std::cout << "== Started run OUTSIDE transaction" << std::endl;
                        }
                        run_txn_ops(&runSession);
                    }
                    if (_isDebug) {
                        std::cout << "== Finished run" << std::endl;
                    }
                } catch (mongocxx::operation_exception e) {
                    std::cout << "error >> " << e.what() << std::endl;
                    BOOST_THROW_EXCEPTION(e);
                }
                return std::nullopt;
            });
        }
    }

private:
    std::string _dbName;
    mongocxx::collection _collection;
    bool _isDebug = false;
    DocumentGenerator _insertDocumentExpr;
    mongocxx::options::transaction _transactionOptions;

    std::vector<ExperimentSetting> _experimentSettings;
};

/** @private */

struct MatViewActor::CollectionName {
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

struct MatViewActor::PhaseConfig {
    explicit PhaseConfig(PhaseContext& context, mongocxx::pool::entry& client, ActorId actorId)
        : operation{context.actor().operation("MatViewPhase", actorId)},
          dbName{getDbName(context)},
          collectionName{context},
          matViewOp(context["Operation"],
                    client,
                    dbName,
                    collectionName.generateName(actorId),
                    context,
                    actorId) {
        // Check if we have Operations or States. Throw an error if we have both.
        if (!context["Operation"]) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("MatViewActor does not have Operation defined."));
        }
    }

    /** record data about each iteration */
    metrics::Operation operation;
    std::string dbName;
    MatViewActor::CollectionName collectionName;
    MatViewOperation matViewOp;
};

void MatViewActor::run() {
    for (auto&& config : _loop) {
        // Note that this gets printed before any rate-limiting occurs.
        // I.e an actor may print "Starting ... execution" then be rate-limited
        // because rate-limiting is part of the inner actor iteration.
        std::cout << "---------- Starting " << this->actorInfo() << " execution" << std::endl;
        for (auto _ : config) {
            auto session = _client->start_session();
            auto metricsContext = config->operation.start();
            config->matViewOp.run(session);
            metricsContext.success();
        }
        std::cout << "---------- Ended " << this->actorInfo() << " execution" << std::endl;
    }
}

MatViewActor::MatViewActor(genny::ActorContext& context)
    : Actor(context),
      _client{std::move(
          context.client(context.get("ClientName").maybe<std::string>().value_or("Default")))},
      _loop{context, _client, MatViewActor::id()},
      _rng{context.workload().getRNGForThread(MatViewActor::id())} {}

namespace {
auto registerMatViewActor = genny::Cast::registerDefault<genny::actor::MatViewActor>();
}


}  // namespace genny::actor
