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

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

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
 * The idea is to create a visitor that is as topology-agnostic as possible,
 * e.g. in a replica set `visitShardedDescription` will never be called.
 */
class TopologyVisitor {
public:
    virtual void visitTopLevelPre(const AbstractTopologyDescription&) {}
    virtual void visitTopLevelPost(const AbstractTopologyDescription&) {}

    virtual void visitMongodDescription(const MongodDescription&) {}
    virtual void visitMongodDescriptionBetween(const MongodDescription&) {}

    virtual void visitMongosDescription(const MongosDescription&) {}

    virtual void visitReplSetDescriptionPre(const ReplSetDescription&) {}
    virtual void visitReplSetDescriptionPost(const ReplSetDescription&) {}

    virtual void visitShardedDescriptionPre(const ShardedDescription&) {}
    virtual void visitShardedDescriptionPost(const ShardedDescription&) {}
    virtual ~TopologyVisitor() {}
};

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
    std::vector<MongodDescription> nodes;

    void accept(TopologyVisitor& v) { 
        v.visitReplSetDescriptionPre(*this); 
        for (int i = 0; i < nodes.size() - 1; i++) {
            nodes[i].accept(v);
            v.visitMongodDescriptionBetween(nodes[i]);
        }
        nodes[nodes.size() - 1].accept(v);
        v.visitReplSetDescriptionPost(*this); 
    }
};

struct ShardedDescription : public AbstractTopologyDescription {
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    MongosDescription mongos;

    void accept(TopologyVisitor& v) { 
        v.visitShardedDescriptionPre(*this); 
        configsvr.accept(v);
        for (auto shard : shards) {
            shard.accept(v);
        }
        mongos.accept(v);
        v.visitShardedDescriptionPost(*this); 
    }
};

/**
 * Represents the topology of a MongoDB cluster.
 */
class Topology {
public:

    /**
     * Traverse the cluster, using the visitor to act on it.
     */
    void accept(TopologyVisitor& v) { 
        v.visitTopLevelPre(*_topology);
        if (_topology) {
            _topology->accept(v); 
        }
        v.visitTopLevelPost(*_topology);
    }

    /**
     * Update the Topology's view of the cluster.
     */
    void update(mongocxx::pool::entry& client);

private:
    void getDataMemberConnectionStrings(mongocxx::pool::entry& client);
    void findConnectedNodesViaMongos(mongocxx::pool::entry& client);
    std::unique_ptr<AbstractTopologyDescription> _topology = nullptr;
};

class ToJsonVisitor : public TopologyVisitor {
public:
    void visitTopLevel(const AbstractTopologyDescription&) {
        result.str("");
        result.clear();
    }
    void visitMongodDescription(const MongodDescription& desc) {
        result << "{mongodUri: " << desc.mongodUri << "}"; 
    }
    void visitMongodDescriptionBetween(const MongodDescription& desc) {
        result << ", ";
    }
    void visitMongosDescription(const MongosDescription&) {}
    void visitReplSetDescriptionPre(const ReplSetDescription& desc) {
        result << "{primaryUri: " << desc.primaryUri << ", nodes: [";
    }

    void visitReplSetDescriptionPost(const ReplSetDescription& desc) {
        result << "]}";
    }

    //void visitShardedDescription(const ShardedDescription&) {}

    std::string str() { return result.str(); }

private:
    std::stringstream result;
};


} // namespace genny

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
