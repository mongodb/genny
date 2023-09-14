// Copyright 2023-present MongoDB Inc.
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

#include <cast_core/actors/DbCheckActor.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/v1/Topology.hpp>


namespace genny::actor {

namespace {
bool clearHealthLog(v1::Topology& topology) {
    class ClearHealthLogVisitor : public v1::TopologyVisitor {
    public:
        void onReplSetMongod(const v1::MongodDescription& desc) override {
            mongocxx::client client(mongocxx::uri(desc.mongodUri));
            auto local = client["local"];
            auto healthLog = local["system.healthlog"];
            healthLog.drop();
            BOOST_LOG_TRIVIAL(debug) << "Finished clearing healthLog on node: " << desc.mongodUri;
            _success = true;
        };

        bool success() {
            return _success;
        }

    private:
        bool _success = false;
    };

    ClearHealthLogVisitor clearHealthLogVisitor;
    topology.accept(clearHealthLogVisitor);
    return clearHealthLogVisitor.success();
}

bool waitForDbCheckToFinish(v1::Topology& topology) {
    class WaitForDbCheckVisitor : public v1::TopologyVisitor {
    public:
        void onReplSetMongod(const v1::MongodDescription& desc) override {
            BOOST_LOG_TRIVIAL(debug) << "Waiting for dbcheck to finish on node: " << desc.mongodUri;

            mongocxx::client client(mongocxx::uri(desc.mongodUri));
            auto local = client["local"];
            auto healthLog = local["system.healthlog"];
            // Dbcheck end health log.
            auto query = bsoncxx::builder::stream::document{} << "operation"
                                                              << "dbCheckStop"
                                                              << bsoncxx::builder::stream::finalize;
            int count = 0;
            while (!count) {
                count = healthLog.count_documents(query.view());
            }

            BOOST_LOG_TRIVIAL(debug) << "dbcheck finished on node: " << desc.mongodUri;
            _success = true;
        };

        bool success() {
            return _success;
        }

    private:
        bool _success = false;
    };

    WaitForDbCheckVisitor waitForDbCheckVisitor;
    topology.accept(waitForDbCheckVisitor);
    return waitForDbCheckVisitor.success();
}

bool dbcheck(mongocxx::pool::entry& client,
             mongocxx::database& db,
             std::string& collName,
             const bsoncxx::document::value& dbCheckCmd) {
    /* Only one thread needs to actually do the dbcheck. If
     * another thread enters the function while a dbcheck
     * is happening, it waits until the dbcheck finishes
     * and exits.
     */
    static std::mutex dbcheckLock;
    static std::atomic_bool success;

    if (dbcheckLock.try_lock()) {
        // We are the thread actually running dbcheck.
        const std::lock_guard<std::mutex> lock(dbcheckLock, std::adopt_lock);
        v1::Topology topology(*client);
        bool clearHealthLogSuccess = clearHealthLog(topology);
        // Run dbcheck.
        db.run_command(dbCheckCmd.view());
        bool waitForDbCheckToFinishSuccess = waitForDbCheckToFinish(topology);
        success = clearHealthLogSuccess && waitForDbCheckToFinishSuccess;
    } else {
        // We are waiting for another thread to do the dbcheck.
        dbcheckLock.lock();
        dbcheckLock.unlock();
    }

    return success;
}

}  // anonymous namespace

struct DbCheckActor::PhaseConfig {
    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db}, 
          collectionName{phaseContext["Collection"].to<std::string>()} {
        auto threads = phaseContext.actor()["Threads"].to<int>();
        if (threads > 1) {
            std::stringstream ss;
            ss << "DbCheckActor only allows 1 thread, but found " << threads << " threads.";
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(ss.str()));
        }

        bsoncxx::builder::stream::document dbCheckCmdStream;
        dbCheckCmdStream << "dbCheck" << collectionName;
        // Adding parameters.
        auto validateMode = phaseContext["ValidateMode"].maybe<std::string>();
        if (validateMode) {
            dbCheckCmdStream << "validateMode" << validateMode.value();
            if (validateMode.value() == "extraIndexKeysCheck") {
                auto secondaryIndex = phaseContext["SecondaryIndex"].to<std::string>();
                dbCheckCmdStream << "secondaryIndex" << secondaryIndex;
            }
        }

        dbCheckCmd = (dbCheckCmdStream << bsoncxx::builder::stream::finalize);
        BOOST_LOG_TRIVIAL(debug) 
            << "About to run dbcheck command: " << bsoncxx::to_json(dbCheckCmd.view());
    }

    mongocxx::database database;
    std::string collectionName;
    bsoncxx::document::value dbCheckCmd = bsoncxx::from_json("{}");
};

void DbCheckActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto dbcheckCtx = _dbcheckMetric.start();
            BOOST_LOG_TRIVIAL(debug) << "DbCheckActor starting dbcheck.";
            if (dbcheck(_client, config->database, config->collectionName, config->dbCheckCmd)) {
                dbcheckCtx.success();
            } else {
                dbcheckCtx.failure();
            }
        }
    }
}

DbCheckActor::DbCheckActor(genny::ActorContext& context)
    : Actor{context},
      _dbcheckMetric{context.operation("DbCheck", DbCheckActor::id())},
      _client{context.client()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], DbCheckActor::id()} {}

namespace {
auto registerDbCheckActor = Cast::registerDefault<DbCheckActor>();
}  // namespace
}  // namespace genny::actor
