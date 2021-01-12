#ifndef HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
#define HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED

#include <vector>
#include <variant>
#include <mutex>

#include <mongocxx/client.hpp>

namespace genny {

struct MongodDescription;

struct AbstractTopologyDescription {
    virtual std::vector<MongodDescription> getNodes() = 0;
    virtual ~AbstractTopologyDescription() = 0;
};

struct MongodDescription {
    std::string mongod;
    virtual std::vector<MongodDescription> getNodes() {
        return std::vector<MongodDescription>{*this};
    }
};

struct MongosDescription {
    std::string mongos;
};

struct ReplSetDescription {
    std::string primary;
    std::vector<MongodDescription> nodes;
    virtual std::vector<MongodDescription> getNodes() {
        std::vector<MongodDescription> output;
        for (auto node : nodes) {
            output.push_back(node);
        }
        return output;
    }
};

struct ShardedDescription {
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    MongosDescription mongos;
    virtual std::vector<MongodDescription> getNodes() {
        std::vector<MongodDescription> output;
        for (auto shard : shards) {
            auto shardNodes = shard.getNodes();
            output.insert(output.end(), shardNodes.begin(), shardNodes.end());
        }
        auto configNodes = configsvr.getNodes();
        output.insert(output.end(), configNodes.begin(), configNodes.end());

        return output;
    }
};

class TopologyDescription {
    /*TopologyDescription(mongocxx::pool::entry& client) {

    }*/

    std::vector<MongodDescription> getNodes() { return _topology->getNodes(); }

private:
    std::unique_ptr<AbstractTopologyDescription> _topology;
};

}

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
