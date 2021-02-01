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

#include <mongocxx/client.hpp>
#include <gennylib/v1/PoolFactory.hpp>

namespace genny::v1 {

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

    void accept(TopologyVisitor& v);
};

struct MongosDescription : public TopologyDescription {
    std::string mongosUri;

    void accept(TopologyVisitor& v);
};

struct ReplSetDescription : public TopologyDescription {
    std::string primaryUri;
    bool configsvr = false;
    std::vector<MongodDescription> nodes;

    void accept(TopologyVisitor& v);
};

struct ShardedDescription : public TopologyDescription {
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    std::vector<MongosDescription> mongoses;

    void accept(TopologyVisitor& v);
};

using ConnectionUri = std::string;

/**
 * Abstraction over DB client access.
 */
class DBConnection{
public:
    virtual ConnectionUri uri() = 0;

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
    ConnectionUri uri();
    bsoncxx::document::value runAdminCommand(std::string command);
    std::unique_ptr<DBConnection> makePeer(ConnectionUri uri);

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
