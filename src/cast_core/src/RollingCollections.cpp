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

#include <cast_core/actors/RollingCollections.hpp>

#include <memory>

#include <bsoncxx/builder/basic/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <metrics/metrics.hpp>
#include <metrics/operation.hpp>

#include <value_generators/DocumentGenerator.hpp>

#include <chrono>
#include <ctime>
#include <iostream>
#include <utility>

namespace genny::actor {
typedef RollingCollections::RollingCollectionNames RollingCollectionNames;

// Pure-virtual interface
struct RunOperation {
    virtual void run() = 0;
    virtual ~RunOperation() = default;

    RunOperation(mongocxx::database db, RollingCollectionNames& rollingCollectionNames)
        : database{std::move(db)}, rollingCollectionNames{rollingCollectionNames} {}

    mongocxx::database database;
    RollingCollectionNames& rollingCollectionNames;
};

static long getMillisecondsSinceEpoch() {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(metrics::clock::now())
        .time_since_epoch()
        .count();
}

static std::string getRollingCollectionName() {
    // The id is tracked globally and increments for every collection created.
    static std::atomic_long id = 0;
    std::stringstream ss;
    // Create a unique collection name that sorts lexographically by time.
    ss << "r_" << getMillisecondsSinceEpoch() << "_" << id;
    id++;
    return ss.str();
}

// Basic linear distribution.
int getNextCollectionId(size_t size, double distribution, double rand) {
    return std::floor(size - ((distribution * rand) * size));
}

mongocxx::collection createCollection(mongocxx::database& database,
                                      std::vector<DocumentGenerator>& indexConfig,
                                      const std::string& collectionName) {
    auto collection = database.create_collection(collectionName);
    for (auto&& keys : indexConfig) {
        collection.create_index(keys());
    }
    return collection;
}

struct Read : public RunOperation {
    Read(PhaseContext& phaseContext,
         mongocxx::database db,
         ActorId id,
         RollingCollectionNames& rollingCollectionNames,
         DefaultRandom& random)
        : RunOperation(std::move(db), rollingCollectionNames),
          _filterExpr{phaseContext["Filter"].maybe<DocumentGenerator>(phaseContext, id)},
          _distribution{phaseContext["Distribution"].maybe<double>().value_or(0)},
          _findOperation{phaseContext.operation("Find", id)},
          _random{random},
          _realDistribution{0, 1} {}

    ~Read() override = default;

    void run() override {
        auto size = rollingCollectionNames.size();
        auto id = getNextCollectionId(size, _distribution, _realDistribution(_random));
        auto statTracker = _findOperation.start();
        if (size <= 0) {
            statTracker.failure();
            return;
        }
        try {
            auto collection = database[rollingCollectionNames.at(id)];
            auto filter =
                _filterExpr ? _filterExpr->evaluate() : bsoncxx::document::view_or_value{};
            auto optionalDocument = collection.find_one(filter);
            if (optionalDocument) {
                auto document = optionalDocument.get();
                statTracker.addDocuments(1);
                statTracker.addBytes(document.view().length());
                statTracker.success();
            } else {
                statTracker.failure();
            }
        } catch (const std::exception& ex) {
            // We likely tried to read from a collection that was deleted.
            // Or we accessed an element from the array which was out of bounds.
            statTracker.failure();
        }
    }


private:
    std::optional<DocumentGenerator> _filterExpr;
    double _distribution;
    metrics::Operation _findOperation;
    boost::random::uniform_real_distribution<double> _realDistribution;
    DefaultRandom& _random;
};

struct Write : public RunOperation {
    Write(PhaseContext& phaseContext,
          mongocxx::database db,
          ActorId id,
          RollingCollectionNames& rollingCollectionNames)
        : RunOperation(std::move(db), rollingCollectionNames),
          _insertOperation{phaseContext.operation("Insert", id)},
          _documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}

    ~Write() override = default;

    void run() override {
        auto statTracker = _insertOperation.start();
        auto document = _documentExpr();
        if (rollingCollectionNames.empty()) {
            statTracker.failure();
            return;
        }
        auto collectionName = rollingCollectionNames.back();
        try {
            auto collection = database[collectionName];
            collection.insert_one(document.view());
            statTracker.addDocuments(1);
            statTracker.addBytes(document.view().length());
            statTracker.success();
        } catch (mongocxx::operation_exception& e) {
            // Theres a small chance our collection won't exist if our window size is 0.
            statTracker.failure();
        }
    }

private:
    DocumentGenerator _documentExpr;
    metrics::Operation _insertOperation;
};

struct Setup : public RunOperation {
    Setup(PhaseContext& phaseContext,
          mongocxx::database db,
          ActorId id,
          RollingCollectionNames& rollingCollectionNames)
        : RunOperation(std::move(db), rollingCollectionNames),
          _documentExpr{phaseContext["Document"].maybe<DocumentGenerator>(phaseContext, id)},
          _documentCount{phaseContext["DocumentCount"].maybe<IntegerSpec>().value_or(0)},
          _collectionWindowSize{phaseContext["CollectionWindowSize"].to<IntegerSpec>()} {
        _indexConfig = std::vector<DocumentGenerator>{};
        auto& indexNodes = phaseContext["Indexes"];
        for (auto [k, indexNode] : indexNodes) {
            _indexConfig.emplace_back(indexNode["keys"].to<DocumentGenerator>(phaseContext, id));
        }
    }

    ~Setup() override = default;

    void run() override {
        BOOST_LOG_TRIVIAL(info) << "Creating " << _collectionWindowSize << " initial collections.";
        for (int i = 0; i < _collectionWindowSize; ++i) {
            auto collectionName = getRollingCollectionName();
            auto collection = createCollection(database, _indexConfig, collectionName);
            rollingCollectionNames.emplace_back(collectionName);
            if (_documentExpr) {
                for (int j = 0; j < _documentCount; ++j) {
                    collection.insert_one(_documentExpr->evaluate());
                }
            }
        }
    }

private:
    std::vector<DocumentGenerator> _indexConfig;
    std::optional<DocumentGenerator> _documentExpr;
    int64_t _collectionWindowSize;
    int64_t _documentCount;
};

struct Manage : public RunOperation {
    Manage(PhaseContext& phaseContext,
           mongocxx::database db,
           ActorId id,
           RollingCollectionNames& rollingCollectionNames)
        : RunOperation(std::move(db), rollingCollectionNames),
          _deleteCollectionOperation{phaseContext.operation("CreateCollection", id)},
          _createCollectionOperation{phaseContext.operation("DeleteCollection", id)},
          _indexConfig{} {
        if (phaseContext.actor()["Threads"].to<int>() != 1) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("Manage can only be run with one thread"));
        }
        _indexConfig = std::vector<DocumentGenerator>{};
        auto& indexNodes = phaseContext["Indexes"];
        for (auto [k, indexNode] : indexNodes) {
            _indexConfig.emplace_back(indexNode["keys"].to<DocumentGenerator>(phaseContext, id));
        }
    }

    ~Manage() override = default;

    void run() override {
        // Delete collection at head of deque, check to see that a collection exists first.
        if (!rollingCollectionNames.empty()) {
            auto collectionName = rollingCollectionNames.front();
            rollingCollectionNames.pop_front();
            auto deleteCollectionTracker = _deleteCollectionOperation.start();
            database[collectionName].drop();
            deleteCollectionTracker.success();
        }
        // Create collection
        auto collectionName = getRollingCollectionName();
        auto createCollectionTracker = _createCollectionOperation.start();
        createCollection(database, _indexConfig, collectionName);
        createCollectionTracker.success();
        rollingCollectionNames.emplace_back(collectionName);
    }

private:
    metrics::Operation _deleteCollectionOperation;
    metrics::Operation _createCollectionOperation;
    std::vector<DocumentGenerator> _indexConfig;
};

struct OplogTailer : public RunOperation {
    // Used only within this class, this tracks best/worst/average lag times.
    struct LagTrack {
        uint64_t best = UINT64_MAX;
        uint64_t worst = 0;
        uint64_t total = 0;
        int count = 0;

        void addLag(uint64_t lag) {
            if (lag < best) {
                best = lag;
            }
            if (lag > worst) {
                worst = lag;
            }
            total += lag;
            count++;
        }

        void clear() {
            best = UINT64_MAX;
            worst = 0;
            total = 0;
            count = 0;
        }
    };

    OplogTailer(PhaseContext& phaseContext,
                mongocxx::database db,
                ActorId id,
                RollingCollectionNames& rollingCollectionNames)
        : RunOperation(db, rollingCollectionNames),
          _cursor{},
          _oplogLagOperation{phaseContext.operation("OplogLag", id)} {
        if (phaseContext.actor()["Threads"].to<int>() != 1) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("OplogTailer can only be run with one thread"));
        }
    }

    std::optional<long> isRollingOplogEntry(const bsoncxx::document::view& doc) {
        if (doc["op"].get_utf8().value.to_string() == "c") {
            auto object = doc["o"].get_document().value;
            auto it = object.find("create");
            if (it != object.end()) {
                auto collectionName = object["create"].get_utf8().value.to_string();
                if (collectionName.substr(0, 2) == "r_") {
                    // It's a collection we care about,
                    // get the embedded millisecond time.
                    std::vector<std::string> timeSplit;
                    boost::algorithm::split(timeSplit, collectionName, boost::is_any_of("_"));
                    return (std::make_optional(stol(timeSplit[1])));
                }
            }
        }
        return (std::nullopt);
    }

    // When we're still catching up, our lag times will be getting better
    // and better. As a simple determinant, we'll say we're done catching
    // up if we haven't a new "best" lag time within the last 5 seen.
    bool caughtUp(uint64_t lagNanoseconds) {
        if (!_caughtUp) {
            if (lagNanoseconds < _catchUpBestLag) {
                _catchUpBestLag = lagNanoseconds;
                _catchUpBestWhen = 0;
            } else if (++_catchUpBestWhen > 5) {
                _caughtUp = true;
                BOOST_LOG_TRIVIAL(info) << "Oplog tailer: caught up";
            }
        }
        return (_caughtUp);
    }

    // Called when we see a create of a rolling collection.
    // rollingMillis is the creation time taken from the name of the collection.
    // We generally want to report the lag, but there's an issue when
    // starting up. We'll see all the rolling collections that have been
    // ever created in the oplog, and we'll have an artificial big spike in
    // latencies. So we determine when we're "catching up", and ignore entries
    // until we are caught up.
    void trackRollingCreate(uint64_t rollingMillis, LagTrack& lagTrack) {
        auto now = metrics::clock::now();
        auto started = metrics::clock::time_point{
            std::chrono::duration_cast<metrics::clock::time_point::duration>(
                std::chrono::milliseconds{rollingMillis})};
        auto lag = std::chrono::duration_cast<std::chrono::microseconds>(now - started);
        auto lagns = lag.count();

        if (caughtUp(lagns)) {
            _oplogLagOperation.report(now, lag, metrics::OutcomeType::kSuccess);
            lagTrack.addLag(lagns);

            // Every minute (60 rolling collection creations), display some
            // simple lag time stats and reset the stats.
            if (lagTrack.count == 60) {
                BOOST_LOG_TRIVIAL(info) << "Oplog tailer lag time stats: best " << lagTrack.best
                                        << "ns, worst " << lagTrack.worst << "ns, average "
                                        << (lagTrack.total / lagTrack.count) << "ns";
                lagTrack.clear();
            }
        }
    }

    // Generally, this method runs once, for a long time, but if the oplog
    // traffic goes completely idle for a second, this method will return,
    // and will be called again by genny if the workload is still running.
    void run() override {
        // We are using the "await" cursor type,
        // which always waits for the next oplog entry (or until a second
        // elapses).  When we reach a steady state, we can determine the
        // lag time in the oplog.  This is done by watching for the
        // creation of rolling collections in the oplog.  Each such
        // collection is named using a time stamp, and by looking
        // at the difference of the current time and the time stamp name,
        // we get the lag time, including the actual creation time.
        mongocxx::options::find opts{};
        opts.cursor_type(mongocxx::cursor::type::k_tailable_await);
        if (!_caughtUp) {
            // first time
            _cursor = std::optional<mongocxx::cursor>(database["oplog.rs"].find({}, opts));
        }

        // Track the best, worst, average lag times, we'll display them
        // periodically to the output.
        LagTrack lagTrack;

        try {
            for (auto&& doc : _cursor.value()) {
                // Look for a creation of a rolling collection.
                auto rollingMillis = isRollingOplogEntry(doc);
                if (rollingMillis) {
                    // Found one, get the time and compute the lag.
                    trackRollingCreate(*rollingMillis, lagTrack);
                }
            }
            // The cursor will complete the iteration loop when there are
            // no oplog updates. Return and let genny decide if the workload
            // is finished or if the system is truly idle. If the latter,
            // run will be called again and we'll pick up where we left off.
            // The cursor generally pauses a second before returning nothing,
            // but sometimes it returns quickly.  So don't report too often.
            _idleCount++;
            long millis = getMillisecondsSinceEpoch();
            if (_idleLastReported + 10000 < millis) {
                BOOST_LOG_TRIVIAL(info) << "Oplog tailer: idle " << _idleCount << " times";
                _idleLastReported = millis;
                _idleCount = 0L;
            } else if (_idleCount % 100 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (mongocxx::operation_exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Oplog tailer exception: " << e.what();
            throw;
        }
    }

private:
    std::optional<mongocxx::cursor> _cursor;
    metrics::Operation _oplogLagOperation;
    bool _caughtUp = false;
    uint64_t _catchUpBestLag = UINT64_MAX;
    int _catchUpBestWhen = 0;
    long _idleLastReported = 0L;
    long _idleCount = 0L;
};

std::unique_ptr<RunOperation> getOperation(const std::string& operation,
                                           PhaseContext& context,
                                           const mongocxx::database& db,
                                           ActorId id,
                                           RollingCollectionNames& rollingCollectionNames,
                                           DefaultRandom& random) {
    if (operation == "Setup") {
        return std::make_unique<Setup>(context, db, id, rollingCollectionNames);
    } else if (operation == "Manage") {
        return std::make_unique<Manage>(context, db, id, rollingCollectionNames);
    } else if (operation == "Read") {
        return std::make_unique<Read>(context, db, id, rollingCollectionNames, random);
    } else if (operation == "Write") {
        return std::make_unique<Write>(context, db, id, rollingCollectionNames);
    } else if (operation == "OplogTailer") {
        return std::make_unique<OplogTailer>(context, db, id, rollingCollectionNames);
    } else {
        BOOST_THROW_EXCEPTION(InvalidConfigurationException("Unknown operation " + operation));
    }
}

struct RollingCollections::PhaseConfig {
    std::unique_ptr<RunOperation> _operation;

    PhaseConfig(PhaseContext& phaseContext,
                mongocxx::database&& db,
                ActorId id,
                RollingCollectionNames& rollingCollectionNames,
                DefaultRandom& random,
                std::string operation)
        : _operation(getOperation(
              std::move(operation), phaseContext, db, id, rollingCollectionNames, random)) {}
};

void RollingCollections::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            config->_operation->run();
        }
    }
}

RollingCollections::RollingCollections(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _collectionNames{
          WorkloadContext::getActorSharedState<RollingCollections, RollingCollectionNames>()},
      _loop{context,
            (*_client)[context["Database"].to<std::string>()],
            RollingCollections::id(),
            WorkloadContext::getActorSharedState<RollingCollections, RollingCollectionNames>(),
            context.workload().getRNGForThread(RollingCollections::id()),
            context["Operation"].to<std::string>()} {}

namespace {
auto registerRollingCollections = Cast::registerDefault<RollingCollections>();
}  // namespace
}  // namespace genny::actor
