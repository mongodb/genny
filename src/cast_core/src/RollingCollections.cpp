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

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

#include <chrono>
#include <ctime>
#include <iostream>

namespace genny::actor {
typedef RollingCollections::RollingCollectionNames RollingCollectionNames;

// Pure-virtual interface
struct RunOperation {
    virtual void run() = 0;
    RunOperation(mongocxx::database db, RollingCollectionNames& rollingCollectionNames)
        : database{db}, rollingCollectionNames{rollingCollectionNames} {}

    mongocxx::database database;
    RollingCollectionNames& rollingCollectionNames;
};

static std::string getRollingCollectionName() {
    // The id is tracked globally and increments for every collection created.
    static std::atomic_long id = 0;
    std::string timestamp(20, '.');
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time = std::chrono::system_clock::to_time_t(now);
    std::strftime(&timestamp[0], timestamp.size(), "%Y-%m-%d-%H:%M:%S", std::localtime(&time));
    /*
     * This replaces the null terminator in the string which is added by strftime and doesn't get
     * removed by the string stream for some reason.
     */
    timestamp[timestamp.size() - 1] = '.';
    std::stringstream ss;
    /*
     * This will create a collection name looking something like: r134_2019-08-30-11:08:19.123,
     * the setw and setfill pad 0's for the ms if its anything < 100 ms.
     */
    ss << "r" << id << "_" << timestamp << std::setfill('0') << std::setw(3) << ms.count();
    id++;
    return ss.str();
}

// Basic linear distribution.
int getNextCollectionId(size_t size, double distribution, double rand) {
    return std::floor(size - ((distribution * rand) * size));
}

mongocxx::collection createCollection(mongocxx::database& database,
                                      std::vector<DocumentGenerator>& indexConfig,
                                      std::string collectionName) {
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
        : RunOperation(db, rollingCollectionNames),
          _filterExpr{phaseContext["Filter"].maybe<DocumentGenerator>(phaseContext, id)},
          _distribution{phaseContext["Distribution"].maybe<double>().value_or(0)},
          _findOperation{phaseContext.operation("Find", id)},
          _random{random},
          _realDistribution{0, 1} {}

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
        : RunOperation(db, rollingCollectionNames),
          _insertOperation{phaseContext.operation("Insert", id)},
          _documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}

    void run() override {
        auto statTracker = _insertOperation.start();
        auto document = _documentExpr();
        if (!rollingCollectionNames.empty()) {
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
        : RunOperation(db, rollingCollectionNames),
          _documentExpr{phaseContext["Document"].maybe<DocumentGenerator>(phaseContext, id)},
          _documentCount{phaseContext["DocumentCount"].maybe<IntegerSpec>().value_or(0)},
          _collectionWindowSize{phaseContext["CollectionWindowSize"].to<IntegerSpec>()} {
        _indexConfig = std::vector<DocumentGenerator>{};
        auto& indexNodes = phaseContext["Indexes"];
        for (auto [k, indexNode] : indexNodes) {
            _indexConfig.emplace_back(indexNode["keys"].to<DocumentGenerator>(phaseContext, id));
        }
    }

    void run() override {
        BOOST_LOG_TRIVIAL(info) << "Creating " << _collectionWindowSize << " initial collections.";
        for (auto i = 0; i < _collectionWindowSize; ++i) {
            auto collectionName = getRollingCollectionName();
            auto collection = createCollection(database, _indexConfig, collectionName);
            rollingCollectionNames.emplace_back(collectionName);
            if (_documentExpr) {
                for (auto j = 0; j < _documentCount; ++j) {
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
        : RunOperation(db, rollingCollectionNames),
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


std::unique_ptr<RunOperation> getOperation(std::string operation,
                                           PhaseContext& context,
                                           mongocxx::database db,
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
        : _operation(
              getOperation(operation, phaseContext, db, id, rollingCollectionNames, random)) {}
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
