// Copyright 2021-present MongoDB Inc.
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

#include <atomic>
#include <mutex>
#include <thread>

#include <bsoncxx/array/view.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/uri.hpp>

#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <gennylib/dbcheck.hpp>
#include <gennylib/v1/Topology.hpp>

// Logic in this file is based on the following two js implementations:
// https://github.com/10gen/workloads/blob/aeaf42b86bb8f1af9bc6ac90198ac0b4ff32bd14/utils/mongoshell.js#L481
// https://github.com/mongodb/mongo-perf/blob/bd8901a2e76d2fb13d2a6a313f7a9e1bf6be9c04/util/utils.js#L384-L387

namespace genny {

const int DROPPED_COLLECTION_RETRIES = 1000;

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

}  // anonymous namespace

bool dbcheck(mongocxx::pool::entry& client,
             mongocxx::database& db,
             std::string& collName,
             bsoncxx::document::value& dbCheckParam) {
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
        bsoncxx::builder::stream::document dbCheckCmdStream;
        dbCheckCmdStream << "dbCheck" << collName;
        // Add the parameters to the dbcheck command.
        for (auto&& element : dbCheckParam.view()) {
            dbCheckCmdStream << element.key() << element.get_value();
        }

        // Convert the new document to bsoncxx::document::value
        auto dbCheckCmd = (dbCheckCmdStream << bsoncxx::builder::stream::finalize);
        BOOST_LOG_TRIVIAL(debug) 
            << "About to run dbcheck command: " << bsoncxx::to_json(dbCheckCmd.view());

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
}  // namespace genny
