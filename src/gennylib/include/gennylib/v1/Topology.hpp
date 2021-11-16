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

#include <mutex>
#include <sstream>
#include <variant>
#include <vector>

#include <gennylib/v1/PoolFactory.hpp>
#include <mongocxx/client.hpp>

namespace genny::v1 {

class TopologyDescription;
class MongodDescription;
class MongosDescription;
class ReplSetDescription;
class ConfigSvrDescription;
class ShardedDescription;

enum class ClusterType;

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

    virtual void onStandaloneMongod(const MongodDescription&) {}
    virtual void onReplSetMongod(const MongodDescription&) {}
    virtual void onConfigSvrMongod(const MongodDescription&) {}

    virtual void onMongos(const MongosDescription&) {}

    virtual void onBeforeReplSet(const ReplSetDescription&) {}
    virtual void onAfterReplSet(const ReplSetDescription&) {}

    virtual void onBeforeShardedCluster(const ShardedDescription&) {}
    virtual void onAfterShardedCluster(const ShardedDescription&) {}


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

    // Called before/after visiting configsvrs.
    virtual void onBeforeConfigSvr(const ConfigSvrDescription&) {}
    virtual void onAfterConfigSvr(const ConfigSvrDescription&) {}

    virtual ~TopologyVisitor() = 0;
};

// Be careful changing the traversal order of the cluster; visitors may depend on it.
struct TopologyDescription {
    virtual void accept(TopologyVisitor&) = 0;
    virtual ~TopologyDescription() = 0;
};

struct MongodDescription : public TopologyDescription {
    ClusterType clusterType;
    std::string mongodUri;

    void accept(TopologyVisitor& v) override;
};

struct MongosDescription : public TopologyDescription {
    std::string mongosUri;

    void accept(TopologyVisitor& v) override;
};

struct ReplSetDescription : public TopologyDescription {
    std::string primaryUri;
    std::vector<MongodDescription> nodes;

    void accept(TopologyVisitor& v) override;
};

struct ConfigSvrDescription : public ReplSetDescription {
    void accept(TopologyVisitor& v) override;
};

struct ShardedDescription : public TopologyDescription {
    ConfigSvrDescription configsvr;
    std::vector<ReplSetDescription> shards;
    std::vector<MongosDescription> mongoses;

    void accept(TopologyVisitor& v) override;
};

using ConnectionUri = std::string;

/**
 * Abstraction over DB client access.
 */
class DBConnection {
public:
    virtual ConnectionUri uri() const = 0;

    virtual bsoncxx::document::value runAdminCommand(std::string command) = 0;

    /**
     * Factory method that returns a peer service of the same concrete type
     * connected to the given uri.
     */
    virtual std::unique_ptr<DBConnection> makePeer(ConnectionUri uri) = 0;

    virtual ~DBConnection() = 0;
};

class MongoConnection : public DBConnection {
public:
    MongoConnection(ConnectionUri uri);
    ConnectionUri uri() const override;
    bsoncxx::document::value runAdminCommand(std::string command) override;
    std::unique_ptr<DBConnection> makePeer(ConnectionUri uri) override;

private:
    mongocxx::client _client;
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
    void accept(TopologyVisitor& v);

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
    void onBeforeTopology(const TopologyDescription&) override;

    void onStandaloneMongod(const MongodDescription& desc) override;
    void onReplSetMongod(const MongodDescription& desc) override;
    void onConfigSvrMongod(const MongodDescription& desc) override;
    void onBetweenMongods(const ReplSetDescription& desc) override;

    void onMongos(const MongosDescription& desc) override;

    void onBeforeReplSet(const ReplSetDescription& desc) override;
    void onAfterReplSet(const ReplSetDescription& desc) override;

    void onBeforeShardedCluster(const ShardedDescription&) override;
    void onAfterShardedCluster(const ShardedDescription&) override;

    void onBeforeConfigSvr(const ConfigSvrDescription&) override;
    void onAfterConfigSvr(const ConfigSvrDescription&) override;

    void onBeforeShards(const ShardedDescription&) override;
    void onBetweenShards(const ShardedDescription&) override;
    void onAfterShards(const ShardedDescription&) override;

    void onBeforeMongoses(const ShardedDescription&) override;
    void onBetweenMongoses(const ShardedDescription&) override;
    void onAfterMongoses(const ShardedDescription&) override;

    std::string str();

private:
    std::stringstream _result;
};

class TopologyError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace genny::v1

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
