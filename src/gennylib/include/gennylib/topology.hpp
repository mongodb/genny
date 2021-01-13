#ifndef HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
#define HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED

#include <vector>
#include <variant>
#include <mutex>

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

namespace genny {

class MongodDescription;
class MongosDescription;
class ReplSetDescription;
class ShardedDescription;

class TopologyVisitor {
public:
    virtual void visitMongodDescription(const MongodDescription&) {}
    virtual void visitMongosDescription(const MongosDescription&) {}
    virtual void visitReplSetDescription(const ReplSetDescription&) {}
    virtual void visitShardedDescription(const ShardedDescription&) {}
    virtual ~TopologyVisitor() {}
};

struct AbstractTopologyDescription {
    virtual void accept(TopologyVisitor&) = 0;
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
        v.visitReplSetDescription(*this); 
        for (auto node : nodes) {
            node.accept(v);
        }
    }
};

struct ShardedDescription : public AbstractTopologyDescription {
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    MongosDescription mongos;

    void accept(TopologyVisitor& v) { 
        v.visitShardedDescription(*this); 
        configsvr.accept(v);
        for (auto shard : shards) {
            shard.accept(v);
        }
        mongos.accept(v);
    }
};

class Topology {
    void accept(TopologyVisitor& v) { _topology->accept(v); }
    void update(mongocxx::pool::entry& client) {}

private:
    std::shared_ptr<AbstractTopologyDescription> _topology;
};

}

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
