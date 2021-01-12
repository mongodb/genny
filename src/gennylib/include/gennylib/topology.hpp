#ifndef HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
#define HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED

#include <vector>
#include <variant>

#include <mongocxx/client.hpp>

namespace genny {

enum class Topology {
    kStandalone,
    kRouter,
    kReplicaSet,
    kShardedCluster
};

struct MongodDescription {
    Topology type = Topology::kStandalone;
    std::string mongod;
};

struct MongosDescription {
    Topology type = Topology::kRouter;
    std::string mongos;
};

struct ReplSetDescription {
    Topology type = Topology::kReplicaSet;
    std::string primary;
    std::vector<MongodDescription> nodes;
};

struct ShardedDescription {
    Topology type = Topology::kShardedCluster;
    ReplSetDescription configsvr;
    std::vector<ReplSetDescription> shards;
    MongosDescription mongos;
};

using TopologyDescription = std::variant<std::monostate, MongodDescription, MongosDescription, 
      ReplSetDescription, ShardedDescription>;

Topology discoverTopology(mongocxx::pool::entry& client) {

    //TODO: remove
    return Topology::kStandalone;
}

}

#endif  // HEADER_6D7AD2AF_7CD1_49E4_9712_741ABD354DFC_INCLUDED
