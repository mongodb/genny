
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

#include <cast_core/actors/CollectionScanner.hpp>
#include <cast_core/actors/OptionsConversion.hpp>

#include <chrono>
#include <memory>

#include <bsoncxx/array/view.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {
static const char* const transientTransactionLabel = "TransientTransactionError";

enum class ScanType { kCount, kSnapshot, kStandard, kPointInTime };
enum class SortOrderType { kSortNone, kSortForward, kSortReverse };
// We don't need a metrics clock, as we're using this for measuring
// the end of phase durations and scan durations.
using SteadyClock = std::chrono::steady_clock;
namespace bson_stream = bsoncxx::builder::stream;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

struct CollectionScanner::PhaseConfig {
    // We keep collection names, not collections here, because the
    // names make sense within the context of a database, and we may
    // have multiple databases.
    std::vector<std::string> collectionNames;
    std::optional<DocumentGenerator> filterExpr;
    std::vector<mongocxx::database> databases;
    std::optional<SteadyClock::time_point> stopPhase;
    bool skipFirstLoop = false;
    metrics::Operation scanOperation;
    metrics::Operation exceptionsCaught;
    metrics::Operation transientExceptions;
    int64_t documents;
    int64_t scanSizeBytes;
    ScanType scanType;
    SortOrderType sortOrder;
    int64_t collectionSkip;
    std::optional<TimeSpec> scanDuration;
    bool scanContinuous = false;
    bool selectClusterTimeOnly = false;
    bool queryCollectionList;
    std::optional<mongocxx::options::transaction> transactionOptions;
    mongocxx::options::find findOptions;

    PhaseConfig(PhaseContext& context,
                const CollectionScanner* actor,
                const std::string& databaseNames,
                int collectionCount,
                int threads,
                bool generateCollectionNames)
        : databases{},
          skipFirstLoop{context["SkipFirstLoop"].maybe<bool>().value_or(false)},
          filterExpr{context["Filter"].maybe<DocumentGenerator>(context, actor->id())},
          scanOperation{context.operation("Scan", actor->id())},
          exceptionsCaught{context.operation("ExceptionsCaught", actor->id())},
          transientExceptions{context.operation("TransientExceptionsCaught", actor->id())},
          documents{context["Documents"].maybe<IntegerSpec>().value_or(0)},
          scanSizeBytes{context["ScanSizeBytes"].maybe<IntegerSpec>().value_or(0)},
          collectionSkip{context["CollectionSkip"].maybe<IntegerSpec>().value_or(0)},
          scanDuration{context["ScanDuration"].maybe<TimeSpec>()},
          scanContinuous{context["ScanContinuous"].maybe<bool>().value_or(false)},
          selectClusterTimeOnly{context["SelectClusterTimeOnly"].maybe<bool>().value_or(false)},
          findOptions{context["FindOptions"].maybe<mongocxx::options::find>().value_or(mongocxx::options::find{})} {
        // The list of databases is comma separated.
        std::vector<std::string> dbnames;
        boost::split(dbnames, databaseNames, [](char c) { return (c == ','); });
        for (const auto& dbname : dbnames) {
            databases.push_back((*actor->_client)[dbname]);
        }
        const auto now = SteadyClock::now();
        const auto duration = context["Duration"].maybe<TimeSpec>();
        if (duration) {
            *stopPhase = now + (SteadyClock::duration)*duration;
        }

        // Initialise scan type enum.
        auto scanTypeString = context["ScanType"].maybe<std::string>().value_or("");
        // Ignore case
        boost::algorithm::to_lower(scanTypeString);
        if (scanTypeString == "count") {
            scanType = ScanType::kCount;
        } else if (scanTypeString == "snapshot") {
            transactionOptions = mongocxx::options::transaction{};
            auto readConcern = mongocxx::read_concern{};
            // The read concern must be "snapshot" to prevent mongod from yielding and breaking the
            // transaction into multiple shorter ones.
            readConcern.acknowledge_level(mongocxx::read_concern::level::k_snapshot);
            transactionOptions->read_concern(readConcern);
            scanType = ScanType::kSnapshot;
        } else if (scanTypeString == "standard") {
            scanType = ScanType::kStandard;
        } else if (scanTypeString == "point-in-time") {
            scanType = ScanType::kPointInTime;
        } else if (!selectClusterTimeOnly) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("ScanType is invalid."));
        }
        if (scanDuration) {
            if (scanDuration->count() < 0) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("ScanDuration must be non-negative."));
            }
            if (scanType != ScanType::kSnapshot) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "ScanDuration must be used with snapshot scans."));
            }
        }
        if (scanContinuous) {
            auto os = std::ostringstream();
            if (scanType != ScanType::kSnapshot) {
                os << "ScanContinuous is only valid with snapshot scans.";
            }
            if (!scanDuration) {
                os <<  "ScanContinuous is only valid with a scan duration.";
            }
            auto err = os.str();
            if (!err.empty()) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(err));
            }
        }
        auto sortOrderString = context["CollectionSortOrder"].maybe<std::string>().value_or("none");
        if (sortOrderString == "none") {
            sortOrder = SortOrderType::kSortNone;
        } else if (sortOrderString == "forward") {
            sortOrder = SortOrderType::kSortForward;
        } else if (sortOrderString == "reverse") {
            sortOrder = SortOrderType::kSortReverse;
        } else {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("CollectionSortOrder is invalid."));
        }
        if (sortOrder == SortOrderType::kSortNone && collectionSkip != 0) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "non-zero CollectionSkip requires a CollectionSortOrder"));
        }

        if (generateCollectionNames) {
            queryCollectionList = false;
            BOOST_LOG_TRIVIAL(info) << " Generating collection names";
            for (const auto& collectionName :
                 distributeCollectionNames(collectionCount, threads, actor->_index)) {
                collectionNames.push_back(collectionName);
            }
        } else {
            queryCollectionList = true;
        }
    }

    void collectionsFromNameList(const mongocxx::database& db,
                                 const std::vector<std::string>& names,
                                 std::vector<mongocxx::collection>& result) {
        std::vector<std::string> nameOrder = names;
        if (sortOrder == SortOrderType::kSortForward) {
            sort(nameOrder.begin(), nameOrder.end(), std::less<>());
        } else if (sortOrder == SortOrderType::kSortReverse) {
            sort(nameOrder.begin(), nameOrder.end(), std::greater<>());
        }
        if (collectionSkip != 0) {
            if (collectionSkip >= nameOrder.size()) {
                return;
            }
            nameOrder =
                std::vector<std::string>(nameOrder.begin() + collectionSkip, nameOrder.end());
        }
        for (const auto& name : nameOrder) {
            auto collection = db[name];
            if (collection) {
                result.push_back(collection);
            }
        }
    }
};

void collectionScan(CollectionScanner::PhaseConfig* config,
                    std::vector<mongocxx::collection>& collections,
                    GlobalRateLimiter* rateLimiter,
                    const mongocxx::client_session& session) {
    /*
     * Here we are either doing a snapshot collection scan
     * or just a normal scan?
     */
    size_t docCount = 0;
    size_t scanSize = 0;
    size_t scanMegabytes = 0;
    bool scanFinished = false;
    auto statTracker = config->scanOperation.start();
    for (auto& collection : collections) {
        auto filter = config->filterExpr ? config->filterExpr->evaluate()
                                         : bsoncxx::document::view_or_value{};
        auto docs = collection.find(session, filter, config->findOptions);
        /*
         * Try-catch this as the collection may have been deleted.
         * You can still do a find but it'll throw an exception when we iterate.
         */
        try {
            for (auto& doc : docs) {
                docCount += 1;
                scanSize += doc.length();
                if (config->documents != 0 && config->documents == docCount) {
                    scanFinished = true;
                    break;
                }
                if (config->scanSizeBytes != 0 && scanSize >= config->scanSizeBytes) {
                    scanFinished = true;
                    break;
                }
                if (rateLimiter && scanSize >= (scanMegabytes + 1) * 1e6) {
                    // Perform an iteration for each megabyte since our rate will be MB/time.
                    rateLimiter->simpleLimitRate();
                    ++scanMegabytes;
                }
            }
            if (scanFinished) {
                break;
            }
        } catch (const mongocxx::operation_exception& e) {
            auto exceptionsCaught = config->exceptionsCaught.start();
            exceptionsCaught.addDocuments(1);
            exceptionsCaught.success();
        }
    }
    statTracker.addBytes(scanSize);
    statTracker.addDocuments(docCount);
    statTracker.success();
}

void countScan(CollectionScanner::PhaseConfig* config,
               std::vector<mongocxx::collection> collections) {
    auto statTracker = config->scanOperation.start();
    for (auto& collection : collections) {
        auto filter = config->filterExpr ? config->filterExpr->evaluate()
                                         : bsoncxx::document::view_or_value{};
        try {
            statTracker.addDocuments(collection.count_documents(filter));
        } catch (const mongocxx::operation_exception& e) {
            auto exceptionsCaught = config->exceptionsCaught.start();
            exceptionsCaught.addDocuments(1);
            exceptionsCaught.success();
        }
    }
    statTracker.success();
}

void pointInTimeScan(mongocxx::pool::entry& client,
                     CollectionScanner::PhaseConfig* config,
                     std::optional<bsoncxx::types::b_timestamp> readClusterTime) {
    int64_t docCount = 0;
    int64_t scanSize = 0;
    bool scanFinished = false;
    mongocxx::options::client_session sessionOption;
    sessionOption.causal_consistency(false);
    mongocxx::client_session session = client->start_session(sessionOption);
    if (!readClusterTime) {
        client->database("admin").run_command(session, make_document(kvp("ping", 1)));
        readClusterTime = session.operation_time();
    }
    bsoncxx::document::value readConcern = bson_stream::document{}
        << "level"
        << "snapshot"
        << "atClusterTime" << readClusterTime.value() << bson_stream::finalize;
    auto filter =
        config->filterExpr ? config->filterExpr->evaluate() : bsoncxx::document::view_or_value{};
    for (auto database : config->databases) {
        auto collectionNames = [&] {
            if (config->queryCollectionList) {
                return database.list_collection_names({});
            } else {
                return config->collectionNames;
            }
        }();
        for (auto& collection : collectionNames) {
            bsoncxx::document::value findCmd = bson_stream::document{}
                << "find" << collection << "filter" << filter << "readConcern" << readConcern.view()
                << bson_stream::finalize;

            int64_t cursorId = 0;
            int count, size;
            auto findTracker = config->scanOperation.start();
            try {
                auto res = database.run_command(session, findCmd.view());
                bsoncxx::array::view batch{res.view()["cursor"]["firstBatch"].get_array().value};
                count = std::distance(batch.begin(), batch.end());
                size = batch.length();
                findTracker.addBytes(size);
                findTracker.addDocuments(count);
                findTracker.success();
                cursorId = res.view()["cursor"]["id"].get_int64();
            } catch (const mongocxx::operation_exception& e) {
                findTracker.failure();
                auto exceptionsCaught = config->exceptionsCaught.start();
                exceptionsCaught.addDocuments(1);
                exceptionsCaught.success();
                break;
            }

            docCount += count;
            scanSize += size;
            if (config->documents != 0 && config->documents == docCount) {
                scanFinished = true;
                break;
            }
            if (config->scanSizeBytes != 0 && scanSize >= config->scanSizeBytes) {
                scanFinished = true;
                break;
            }

            while (cursorId) {
                bsoncxx::document::value getMoreCmd = bson_stream::document{}
                    << "getMore" << cursorId << "collection" << collection << bson_stream::finalize;

                auto getMoreTracker = config->scanOperation.start();
                try {
                    auto res = database.run_command(session, getMoreCmd.view());
                    bsoncxx::array::view batch{res.view()["cursor"]["nextBatch"].get_array().value};
                    count = std::distance(batch.begin(), batch.end());
                    size = batch.length();
                    getMoreTracker.addBytes(size);
                    getMoreTracker.addDocuments(count);
                    getMoreTracker.success();
                    cursorId = res.view()["cursor"]["id"].get_int64();
                } catch (const mongocxx::operation_exception& e) {
                    getMoreTracker.failure();
                    auto exceptionsCaught = config->exceptionsCaught.start();
                    exceptionsCaught.addDocuments(1);
                    exceptionsCaught.success();
                    break;
                }

                docCount += count;
                scanSize += size;
                if (config->documents != 0 && config->documents == docCount) {
                    scanFinished = true;
                    break;
                }
                if (config->scanSizeBytes != 0 && scanSize >= config->scanSizeBytes) {
                    scanFinished = true;
                    break;
                }
            }
            if (scanFinished) {
                break;
            }
        }
        if (scanFinished) {
            break;
        }
    }
}

void CollectionScanner::run() {
    int i = 0;
    std::optional<bsoncxx::types::b_timestamp> readClusterTime;
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->selectClusterTimeOnly) {
                std::this_thread::sleep_for(std::chrono::seconds{1});
                auto session = _client->start_session({});
                _client->database("admin").run_command(session, make_document(kvp("ping", 1)));
                readClusterTime = session.operation_time();
                continue;
            }
            if (config->skipFirstLoop) {
                config->skipFirstLoop = false;
                std::this_thread::sleep_for(std::chrono::seconds{1});
                continue;
            }
            _runningActorCounter++;
            // Populate collection names if need be.
            std::vector<mongocxx::collection> collections;
            for (auto database : config->databases) {
                if (config->queryCollectionList) {
                    config->collectionsFromNameList(
                        database, database.list_collection_names({}), collections);
                } else {
                    config->collectionsFromNameList(database, config->collectionNames, collections);
                }
            }
            BOOST_LOG_TRIVIAL(debug) << "Starting collection scanner databases: \"" << _databaseNames
                                     << "\", id: " << this->_index << " " << config->collectionNames.size();


            const SteadyClock::time_point started = SteadyClock::now();

            // Do each kind of scan.
            if (config->scanType == ScanType::kCount) {
                countScan(config, collections);
            } else if (config->scanType == ScanType::kSnapshot) {
                try {
                    mongocxx::client_session session = _client->start_session({});
                    session.start_transaction(*config->transactionOptions);
                    // Initialize 'finished' to true so that the loop will execute exactly once if
                    // ScanContinuous is false. If ScanContinuous is true 'finished' is
                    // re-evaluated within the loop.
                    auto finished = true;
                    do {
                        BOOST_LOG_TRIVIAL(debug)
                            << "Scanner id: " << this->_index << " scanning";
                        collectionScan(config, collections, _rateLimiter, session);
                        // If a scan duration was specified, we must make the scan
                        // last at least that long.
                        // For non-continuous scans (default behaviour) we'll do this within any
                        // running transaction by keeping the "long running transaction" active
                        // as long as we've been asked to.
                        // For continuous scans we repeat the collectionScan
                        // within the transaction until we have reached the scan duration.
                        // However, honor the phase's duration if specified.
                        if (config->scanDuration) {
                            const SteadyClock::time_point now = SteadyClock::now();
                            auto stop = started + (*config->scanDuration).value;
                            if (config->stopPhase && *config->stopPhase < stop) {
                                stop = *config->stopPhase;
                            }
                            finished = now >= stop;
                            if (!finished && !config->scanContinuous) {
                                auto sleepDuration = stop - now;
                                auto secs = sleepDuration.count() / (1000 * 1000 * 1000);
                                BOOST_LOG_TRIVIAL(debug)
                                    << "Scanner id: " << this->_index << " sleeping " << secs;
                                std::this_thread::sleep_for(sleepDuration);
                                finished = true;
                            }
                        }
                    } while(!finished);
                    session.commit_transaction();
                } catch (const mongocxx::operation_exception& e) {
                    if(e.has_error_label(transientTransactionLabel) ) {
                        BOOST_LOG_TRIVIAL(debug) << "Snapshot Scanner operation exception: " << e.what();
                        auto transientExceptions = config->transientExceptions.start();
                        transientExceptions.addDocuments(1);
                        transientExceptions.success();
                    } else {
                        throw;
                    }
                }
            } else if (config->scanType == ScanType::kPointInTime) {
                pointInTimeScan(_client, config, readClusterTime);
            } else {
                const mongocxx::client_session session = _client->start_session({});
                collectionScan(config, collections, _rateLimiter, session);
            }

            _runningActorCounter--;
            BOOST_LOG_TRIVIAL(debug) << "Finished collection scanner id: " << this->_index;
        }
    }
}

CollectionScanner::CollectionScanner(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", CollectionScanner::id())},
      _client{context.client()},
      _index{WorkloadContext::getActorSharedState<CollectionScanner, ActorCounter>().fetch_add(1)},
      _runningActorCounter{
          WorkloadContext::getActorSharedState<CollectionScanner, RunningActorCounter>()},
      _generateCollectionNames{context["GenerateCollectionNames"].maybe<bool>().value_or(false)},
      _databaseNames{context["Database"].to<std::string>()},
      _loop{context,
            this,
            _databaseNames,
            context["CollectionCount"].maybe<IntegerSpec>().value_or(0),
            context["Threads"].to<IntegerSpec>(),
            context["GenerateCollectionNames"].maybe<bool>().value_or(false)} {
    _runningActorCounter.store(0);

    const auto scanRateMegabytes = context["ScanRateMegabytes"].maybe<RateSpec>();
    _rateLimiter = scanRateMegabytes
        ? context.workload().getRateLimiter("CollectionScanner", *scanRateMegabytes)
        : nullptr;
}

std::vector<std::string> distributeCollectionNames(size_t collectionCount,
                                                   size_t threadCount,
                                                   ActorId actorId) {
    // We always want a fair division of collections to actors
    std::vector<std::string> collectionNames{};
    if ((threadCount > collectionCount && threadCount % collectionCount != 0) ||
        collectionCount % threadCount != 0) {
        throw std::invalid_argument("Thread count must be mutliple of database collection count");
    }
    int collectionsPerActor = threadCount > collectionCount ? 1 : collectionCount / threadCount;
    int collectionIndexStart = (actorId % collectionCount) * collectionsPerActor;
    int collectionIndexEnd = collectionIndexStart + collectionsPerActor;
    for (int i = collectionIndexStart; i < collectionIndexEnd; ++i) {
        collectionNames.push_back("Collection" + std::to_string(i));
    }
    return collectionNames;
}

namespace {
auto registerCollectionScanner = Cast::registerDefault<CollectionScanner>();
}  // namespace
}  // namespace genny::actor
