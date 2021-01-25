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

class AbstractTopologyDescription;
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
    virtual void visitTopLevelPre(const AbstractTopologyDescription&) {}
    virtual void visitTopLevelPost(const AbstractTopologyDescription&) {}

    virtual void visitMongodDescription(const MongodDescription&) {}
    virtual void visitMongosDescription(const MongosDescription&) {}

    virtual void visitReplSetDescriptionPre(const ReplSetDescription&) {}
    virtual void visitReplSetDescriptionPost(const ReplSetDescription&) {}

    virtual void visitShardedDescriptionPre(const ShardedDescription&) {}
    virtual void visitShardedDescriptionPost(const ShardedDescription&) {}


    /** Misc hooks that most visitors won't need. **/

    // Called between mongods in a replica set.
    virtual void visitMongodDescriptionBetween(const ReplSetDescription&) {}

    // Called before/after/between visiting shards.
    virtual void visitShardsPre(const ShardedDescription&) {}
    virtual void visitShardsPost(const ShardedDescription&) {}
    virtual void visitShardsBetween(const ShardedDescription&) {}

    // Called before/after/between visiting mongoses.
    virtual void visitMongosesPre(const ShardedDescription&) {}
    virtual void visitMongosesPost(const ShardedDescription&) {}
    virtual void visitMongosesBetween(const ShardedDescription&) {}

    virtual ~TopologyVisitor() {}
};

// Be careful changing the traversal order of the cluster; visitors may depend on it.
struct AbstractTopologyDescription {
    virtual void accept(TopologyVisitor&) = 0;
    virtual ~AbstractTopologyDescription() {};
};

struct MongodDescription : public AbstractTopologyDescription {
    std::string mongodUri;

    void accept(TopologyVisitor& v) { v.visitMongodDescription(*this); }
};

struct MongosDescription : public AbstractTopologyDescription {
    std::string mongosUri;

    void accept(TopologyVisitor& v) { v.visitMongosDescription(*this); }
};

struct ReplSetDescription : public AbstractTopologyDescription {
    std::string primaryUri;
    bool configsvr = false;
    std::vector<MongodDescription> nodes;

    void accept(TopologyVisitor& v) { 
        v.visitReplSetDescriptionPre(*this); 
        for (int i = 0; i < nodes.size() - 1; i++) {
            nodes[i].accept(v);
            v.visitMongodDescriptionBetween(*this);
        }
        nodes[nodes.size() - 1].accept(v);
        v.visitReplSetDescriptionPost(*this); 
    }
};

struct ShardedDescription : public AbstractTopologyDescription {
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    std::vector<MongosDescription> mongoses;

    void accept(TopologyVisitor& v) { 
        v.visitShardedDescriptionPre(*this); 
        configsvr.accept(v);

        v.visitShardsPre(*this); 
        for (int i = 0; i < shards.size() - 1; i++) {
            shards[i].accept(v);
            v.visitShardsBetween(*this);
        }
        shards[shards.size() - 1].accept(v);
        v.visitShardsPost(*this); 

        v.visitMongosesPre(*this);
        for (int i = 0; i < mongoses.size() - 1; i++) {
            mongoses[i].accept(v);
            v.visitMongosesBetween(*this);
        }
        mongoses[mongoses.size() - 1].accept(v);
        v.visitMongosesPost(*this);

        v.visitShardedDescriptionPost(*this); 
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
        if (_topology) {
            v.visitTopLevelPre(*_topology);
            _topology->accept(v); 
            v.visitTopLevelPost(*_topology);
        }
    }

    /**
     * Update the Topology's view of the cluster.
     */
    void update(DBConnection& connection);

private:
    v1::PoolFactory _factory;
    std::string nameToUri(const std::string& name);
    void getDataMemberConnectionStrings(DBConnection& connection);
    void findConnectedNodesViaMongos(DBConnection& connection);
    std::unique_ptr<AbstractTopologyDescription> _topology = nullptr;
};

class ToJsonVisitor : public TopologyVisitor {
public:
    void visitTopLevelPre(const AbstractTopologyDescription&) {
        result.str("");
        result.clear();
    }

    void visitMongodDescription(const MongodDescription& desc) { result << "{mongodUri: " << desc.mongodUri << "}"; }
    void visitMongodDescriptionBetween(const ReplSetDescription& desc) { result << ", "; }

    void visitMongosDescription(const MongosDescription& desc) { result << "{mongosUri: " << desc.mongosUri << "}"; }
    void visitMongosDescriptionBetween(const ReplSetDescription& desc) { result << ", "; }

    void visitReplSetDescriptionPre(const ReplSetDescription& desc) {
        if (desc.configsvr) {
            result << "configsvr: ";
        }
        result << "{primaryUri: " << desc.primaryUri << ", nodes: [";
    }
    void visitReplSetDescriptionPost(const ReplSetDescription& desc) { result << "]}"; }

    void visitShardedDescriptionPre(const ShardedDescription&) { result << "{"; }
    void visitShardedDescriptionPost(const ShardedDescription&) { result << "}"; }

    void visitShardsPre(const ShardedDescription&) { result << " shards: ["; }
    void visitShardsBetween(const ShardedDescription&) { result << ", "; }
    void visitShardsPost(const ShardedDescription&) { result << "], "; }

    void visitMongosesPre(const ShardedDescription&) { result << " mongoses: ["; }
    void visitMongosesBetween(const ShardedDescription&) { result << ", "; }
    void visitMongosesPost(const ShardedDescription&) { result << "]"; }

    std::string str() { return result.str(); }

private:
    std::stringstream result;
};


} // namespace genny

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
