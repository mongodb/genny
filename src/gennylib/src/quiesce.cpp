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

#include <string_view>
#include <thread>
#include <mutex>
#include <atomic>

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/topology.hpp>

namespace genny {

const int DROPPED_COLLECTION_RETRIES = 1000;

namespace {

bool waitOplog(Topology& topology) {
    class WaitOplogVisitor : public TopologyVisitor {
    public:
        void onBeforeReplSet(const ReplSetDescription& desc) {
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
                        BOOST_LOG_TRIVIAL(error) << "Cannot wait oplog, replset member "
                            << name << " is " << state;
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

        bool success() { return _successAcc; }
    private:
        bool _successAcc = true;
    };

    WaitOplogVisitor waitVisitor;
    topology.accept(waitVisitor);

    return waitVisitor.success();
}

void doFsync(Topology& topology) {
    class DoFsyncVisitor : public TopologyVisitor {
        void visitMongodDescription(const MongodDescription& desc) {
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

bool checkForDroppedCollections(mongocxx::database db) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    auto res = db.run_command(make_document(kvp("serverStatus", 1)));
    auto idents = res.view()["storageEngine"]["dropPendingIdents"];

    return idents.get_int64() != 0;
}

bool checkForDroppedCollectionsTestDB(mongocxx::pool::entry& client, std::string dbName) {
    auto db = client->database(dbName);
    int retries = 0;
    while (checkForDroppedCollections(db) && retries < DROPPED_COLLECTION_RETRIES) {
        BOOST_LOG_TRIVIAL(debug) << "Sleeping 1 second while waiting for collection to finish dropping.";
        retries++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (retries >= DROPPED_COLLECTION_RETRIES) {
        BOOST_LOG_TRIVIAL(error) << "Timeout on waiting for collections to drop.";
        return false;
    }
    return true;
}

} // anonymous namespace

// Only one thread needs to actually do the quiesce.
bool quiesce(mongocxx::pool::entry& client, std::string dbName) {
    static std::mutex quiesceLock;
    static std::atomic_bool success;

    if (quiesceLock.try_lock()) {
        Topology topology(*client);
        bool waitOplogSuccess = waitOplog(topology);
        doFsync(topology);
        bool checkDroppedCollectionSuccess = checkForDroppedCollectionsTestDB(client, dbName);
        success = waitOplogSuccess && checkDroppedCollectionSuccess;
        quiesceLock.unlock();
    } else {
        quiesceLock.lock();
        quiesceLock.unlock();
    }

    return success;
}

} // namespace genny
