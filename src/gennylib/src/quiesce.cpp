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

#include <gennylib/quiesce.hpp>
#include <gennylib/v1/Topology.hpp>

// Logic in this file is based on the following two js implementations:
// https://github.com/10gen/workloads/blob/aeaf42b86bb8f1af9bc6ac90198ac0b4ff32bd14/utils/mongoshell.js#L481
// https://github.com/mongodb/mongo-perf/blob/bd8901a2e76d2fb13d2a6a313f7a9e1bf6be9c04/util/utils.js#L384-L387

namespace genny {

const int DROPPED_COLLECTION_RETRIES = 1000;

namespace {

bool waitOplog(v1::Topology& topology) {
    class WaitOplogVisitor : public v1::TopologyVisitor {
    public:
        void onBeforeReplSet(const v1::ReplSetDescription& desc) override {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;
            mongocxx::client client(mongocxx::uri(desc.primaryUri));
            auto admin = client["admin"];

            // Assert that all nodes are in reasonable states.
            auto replSetRes = admin.run_command(make_document(kvp("replSetGetStatus", 1)));
            auto members = replSetRes.view()["members"];
            if (members && members.type() == bsoncxx::type::k_array) {
                bsoncxx::array::view members_view = members.get_array();
                for (auto member : members_view) {
                    std::string state(member["stateStr"].get_utf8().value);
                    if (state != "PRIMARY" && state != "SECONDARY" && state != "ARBITER") {
                        std::string name(member["name"].get_utf8().value);
                        BOOST_LOG_TRIVIAL(warning)
                            << "Cannot wait oplog, replset member " << name << " is " << state;
                        _successAcc = false;
                        return;
                    }
                }
            }

            // Do flush. We do a write concern of "all" and journaling
            // enabled, so when the write returns we know everything
            // before it is replicated.
            auto collection = admin["wait_oplog"];
            mongocxx::write_concern wc;
            wc.nodes(desc.nodes.size());
            wc.journal(true);
            mongocxx::options::insert opts;
            opts.write_concern(wc);
            auto insertRes = collection.insert_one(make_document(kvp("x", "flush")), opts);

            _successAcc = _successAcc && insertRes && insertRes->result().inserted_count() == 1;
        }

        bool success() {
            return _successAcc;
        }

    private:
        bool _successAcc = true;
    };

    WaitOplogVisitor waitVisitor;
    topology.accept(waitVisitor);

    return waitVisitor.success();
}

bool CheckForDroppedCollections(v1::Topology& topology, std::string dbName, SleepContext sleepContext) {
    class CheckForDroppedCollectionsVisitor : public v1::TopologyVisitor {
    public:
        CheckForDroppedCollectionsVisitor(std::string dbName, SleepContext sleepContext)
            : _dbName{dbName},
              _sleepContext{sleepContext} {}

        void onBeforeReplSet(const v1::ReplSetDescription& desc) override {
            mongocxx::client client(mongocxx::uri(desc.primaryUri));
            _successAcc = checkCollectionsTestDB(client);
        }

        bool success() {
            return _successAcc;
        }

    private:
        bool checkCollections(mongocxx::database db) {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;

            auto res = db.run_command(make_document(kvp("serverStatus", 1)));
            if (!res.view()["storageEngine"] || !res.view()["storageEngine"]["dropPendingIdents"]) {
                return false;
            }
            auto idents = res.view()["storageEngine"]["dropPendingIdents"];

            return idents.get_int64() != 0;
        }

        bool checkCollectionsTestDB(mongocxx::client& client) {
            auto db = client.database(_dbName);
            int retries = 0;
            while (checkCollections(db) && retries < DROPPED_COLLECTION_RETRIES) {
                BOOST_LOG_TRIVIAL(debug)
                    << "Sleeping 1 second while waiting for collection to finish dropping.";
                retries++;
                _sleepContext.sleep_for(std::chrono::seconds(1));
            }
            if (retries >= DROPPED_COLLECTION_RETRIES) {
                BOOST_LOG_TRIVIAL(error) << "Timeout on waiting for collections to drop. "
                                         << "Tried " << retries << " >= " << DROPPED_COLLECTION_RETRIES
                                         << " max.";
                return false;
            }
            return true;
        }

        bool _successAcc = true;
        std::string _dbName;
        SleepContext _sleepContext;
    };

    CheckForDroppedCollectionsVisitor CheckForDroppedCollectionsVisitor(dbName, sleepContext);
    topology.accept(CheckForDroppedCollectionsVisitor);

    return CheckForDroppedCollectionsVisitor.success();
}

void doFsync(v1::Topology& topology) {
    class DoFsyncVisitor : public v1::TopologyVisitor {
        void onMongod(const v1::MongodDescription& desc) override {
            using bsoncxx::builder::basic::kvp;
            using bsoncxx::builder::basic::make_document;

            mongocxx::client client(mongocxx::uri(desc.mongodUri));
            auto admin = client["admin"];
            admin.run_command(make_document(kvp("fsync", 1)));
        }
    };
    DoFsyncVisitor v;
    topology.accept(v);
}

}  // anonymous namespace

bool quiesce(mongocxx::pool::entry& client,
             const std::string& dbName,
             const SleepContext& sleepContext) {
    /* Only one thread needs to actually do the quiesce. If
     * another thread enters the function while a quiesce
     * is happening, it waits until the quiesce finishes
     * and exits.
     */
    static std::mutex quiesceLock;
    static std::atomic_bool success;

    if (quiesceLock.try_lock()) {
        // We are the thread actually quiescing.
        const std::lock_guard<std::mutex> lock(quiesceLock, std::adopt_lock);
        v1::Topology topology(*client);
        bool waitOplogSuccess = waitOplog(topology);
        doFsync(topology);
        bool checkDroppedCollectionSuccess =
            CheckForDroppedCollections(topology, dbName, sleepContext);
        success = waitOplogSuccess && checkDroppedCollectionSuccess;
    } else {
        // We are waiting for another thread to do the quiesce.
        quiesceLock.lock();
        quiesceLock.unlock();
    }

    return success;
}

}  // namespace genny
