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


#ifndef HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
#define HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED

#include <vector>
#include <variant>
#include <mutex>
#include <sstream>

#include <gennylib/v1/PoolFactory.hpp>
#include <gennylib/Connection.hpp>

namespace genny {

class TopologyDescription;
class MongodDescription;
class MongosDescription;
class ReplSetDescription;
class ShardedDescription;

/**
 * Inherit from TopologyVisitor and override particular visit methods
 * to implement an algorithm that operates on a cluster. Pass the visitor
 * to a Topology object to execute.
 *
 * The idea is to create a visitor that focuses on each part of the cluster,
 * allow the Topology object to handle traversal, and keep application-level
 * code as topology-agnostic as possible.
 */
class TopologyVisitor {
public:
    virtual void onBeforeTopology(const TopologyDescription&) {}
    virtual void onAfterTopology(const TopologyDescription&) {}

    virtual void onMongod(const MongodDescription&) {}
    virtual void onMongos(const MongosDescription&) {}

    virtual void onBeforeReplSet(const ReplSetDescription&) {}
    virtual void onAfterReplSet(const ReplSetDescription&) {}

    virtual void onBeforeSharded(const ShardedDescription&) {}
    virtual void onAfterSharded(const ShardedDescription&) {}


    /** Misc hooks that most visitors won't need. **/

    // Called between mongods in a replica set.
    virtual void onBetweenMongods(const ReplSetDescription&) {}

    // Called before/after/between visiting shards.
    virtual void onBeforeShards(const ShardedDescription&) {}
    virtual void onAfterShards(const ShardedDescription&) {}
    virtual void onBetweenShards(const ShardedDescription&) {}

    // Called before/after/between visiting mongoses.
    virtual void onBeforeMongoses(const ShardedDescription&) {}
    virtual void onAfterMongoses(const ShardedDescription&) {}
    virtual void onBetweenMongoses(const ShardedDescription&) {}

    virtual ~TopologyVisitor() = 0;
};

// Be careful changing the traversal order of the cluster; visitors may depend on it.
struct TopologyDescription {
    virtual void accept(TopologyVisitor&) = 0;
    virtual ~TopologyDescription() = 0;
};

struct MongodDescription : public TopologyDescription {
    std::string mongodUri;

    void accept(TopologyVisitor& v) { v.onMongod(*this); }
};

struct MongosDescription : public TopologyDescription {
    std::string mongosUri;

    void accept(TopologyVisitor& v) { v.onMongos(*this); }
};

struct ReplSetDescription : public TopologyDescription {
    std::string primaryUri;
    bool configsvr = false;
    std::vector<MongodDescription> nodes;

    void accept(TopologyVisitor& v) { 
        v.onBeforeReplSet(*this); 
        for (int i = 0; i < nodes.size() - 1; i++) {
            nodes[i].accept(v);
            v.onBetweenMongods(*this);
        }
        nodes[nodes.size() - 1].accept(v);
        v.onAfterReplSet(*this); 
    }
};

struct ShardedDescription : public TopologyDescription {
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    std::vector<MongosDescription> mongoses;

    void accept(TopologyVisitor& v) { 
        v.onBeforeSharded(*this); 
        configsvr.accept(v);

        v.onBeforeShards(*this); 
        for (int i = 0; i < shards.size() - 1; i++) {
            shards[i].accept(v);
            v.onBetweenShards(*this);
        }
        shards[shards.size() - 1].accept(v);
        v.onAfterShards(*this); 

        v.onBeforeMongoses(*this);
        for (int i = 0; i < mongoses.size() - 1; i++) {
            mongoses[i].accept(v);
            v.onBetweenMongoses(*this);
        }
        mongoses[mongoses.size() - 1].accept(v);
        v.onAfterMongoses(*this);

        v.onAfterSharded(*this); 
    }
};

/**
 * Represents the topology of a MongoDB cluster.
 */
class Topology {
public:
    Topology(mongocxx::client& client) : _factory{client.uri().to_string()} {
        MongoConnection connection(client.uri().to_string());
        update(connection);
    }

    Topology(DBConnection& connection) : _factory{connection.uri()} {
        update(connection);
    }

    /**
     * Traverse the cluster, using the visitor to act on it.
     */
    void accept(TopologyVisitor& v) { 
        if (_topologyDesc) {
            v.onBeforeTopology(*_topologyDesc);
            _topologyDesc->accept(v); 
            v.onAfterTopology(*_topologyDesc);
        }
    }

    /**
     * Update the Topology's view of the cluster.
     */
    void update(DBConnection& connection);

private:
    v1::PoolFactory _factory;
    std::string nameToUri(const std::string& name);
    void computeDataMemberConnectionStrings(DBConnection& connection);
    void findConnectedNodesViaMongos(DBConnection& connection);
    std::unique_ptr<TopologyDescription> _topologyDesc = nullptr;
};

class ToJsonVisitor : public TopologyVisitor {
public:
    void onBeforeTopology(const TopologyDescription&);

    void onMongod(const MongodDescription& desc);
    void onBetweenMongods(const ReplSetDescription& desc);

    void onMongos(const MongosDescription& desc);
    void onBetweenMongoses(const ReplSetDescription& desc);

    void onBeforeReplSet(const ReplSetDescription& desc);
    void onAfterReplSet(const ReplSetDescription& desc);

    void onBeforeSharded(const ShardedDescription&);
    void onAfterSharded(const ShardedDescription&);

    void onBeforeShards(const ShardedDescription&);
    void onBetweenShards(const ShardedDescription&);
    void onAfterShards(const ShardedDescription&);

    void onBeforeMongoses(const ShardedDescription&);
    void onBetweenMongoses(const ShardedDescription&);
    void onAfterMongoses(const ShardedDescription&);

    std::string str();

private:
    std::stringstream _result;
};


} // namespace genny

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
