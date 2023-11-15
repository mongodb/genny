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

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <gennylib/v1/Topology.hpp>

// Logic in this file is based on the js topology discovery:
// https://github.com/mongodb/mongo/blob/r4.1.4/jstests/libs/discover_topology.js

namespace genny::v1 {

enum class ClusterType {
    standalone,
    replSetMember,
    configSvrMember
};

TopologyVisitor::~TopologyVisitor() {}

TopologyDescription::~TopologyDescription() {}

void MongodDescription::accept(TopologyVisitor& v) {
    if (clusterType == ClusterType::standalone) {
        v.onStandaloneMongod(*this);
    } else if (clusterType == ClusterType::replSetMember) {
        v.onReplSetMongod(*this);
    } else {
        v.onConfigSvrMongod(*this);
    }
}

void MongosDescription::accept(TopologyVisitor& v) {
    v.onMongos(*this);
}

void ReplSetDescription::accept(TopologyVisitor& v) {
    v.onBeforeReplSet(*this);
    for (int i = 0; i < nodes.size() - 1; i++) {
        nodes[i].accept(v);
        v.onBetweenMongods(*this);
    }
    nodes[nodes.size() - 1].accept(v);
    v.onAfterReplSet(*this);
}

void ConfigSvrDescription::accept(TopologyVisitor& v) {
    if (!nodes.empty()) {
        v.onBeforeConfigSvr(*this);
        for (int i = 0; i < nodes.size() - 1; i++) {
            nodes[i].accept(v);
            v.onBetweenMongods(*this);
        }
        nodes[nodes.size() - 1].accept(v);
        v.onAfterConfigSvr(*this);
    }
}

void ShardedDescription::accept(TopologyVisitor& v) {
    v.onBeforeShardedCluster(*this);
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

    v.onAfterShardedCluster(*this);
}

DBConnection::~DBConnection() {}

MongoConnection::MongoConnection(ConnectionUri uri) : _client(mongocxx::uri(uri)) {}

ConnectionUri MongoConnection::uri() const {
    return _client.uri().to_string();
}

bsoncxx::document::value MongoConnection::runAdminCommand(std::string command) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    auto admin = _client.database("admin");
    return admin.run_command(make_document(kvp(command, 1)));
}

std::unique_ptr<DBConnection> MongoConnection::makePeer(ConnectionUri uri) {
    return std::make_unique<MongoConnection>(uri);
}

std::string Topology::nameToUri(const std::string& name) {
    std::set<std::string> nameSet;
    // Hostnames returned from some commands have the form configRepl/hostname:port

    auto strippedName = name;
    if (name.find('/') != std::string::npos) {
        strippedName = name.substr(name.find('/') + 1, std::string::npos);
    }
    nameSet.insert(strippedName);
    _factory.overrideHosts(nameSet);
    return _factory.makeUri();
}

void Topology::accept(TopologyVisitor& v) {
    if (_topologyDesc) {
        v.onBeforeTopology(*_topologyDesc);
        _topologyDesc->accept(v);
        v.onAfterTopology(*_topologyDesc);
    }
}

void Topology::computeDataMemberConnectionStrings(DBConnection& connection) {
    auto res = connection.runAdminCommand("isMaster");
    if (!res.view()["setName"]) {
        std::unique_ptr<MongodDescription> desc = std::make_unique<MongodDescription>();
        desc->clusterType = ClusterType::standalone;
        desc->mongodUri = connection.uri();
        this->_topologyDesc.reset(desc.release());
        return;
    }

    std::unique_ptr<ReplSetDescription> desc;
    std::string setName = res.view()["setName"].get_string().value.data();
    std::set<std::string> configSetNames{"configSet", "configRepl", "configSvrRS"};
    if (configSetNames.find(setName) != configSetNames.end()) {
        desc = std::make_unique<ConfigSvrDescription>();
    } else {
        desc = std::make_unique<ReplSetDescription>();
    }
    auto primary = res.view()["primary"];

    desc->primaryUri = nameToUri(std::string(primary.get_string().value));

    auto hosts = res.view()["hosts"];
    if (hosts && hosts.type() == bsoncxx::type::k_array) {
        bsoncxx::array::view hosts_view = hosts.get_array();
        for (auto member : hosts_view) {
            MongodDescription memberDesc;
            memberDesc.mongodUri = nameToUri(std::string(member.get_string().value));
            if (configSetNames.find(setName) != configSetNames.end()) {
                memberDesc.clusterType = ClusterType::configSvrMember;
            } else {
                memberDesc.clusterType = ClusterType::replSetMember;
            }

            desc->nodes.push_back(memberDesc);
        }
    }

    // The "passives" field contains the list of unelectable (priority=0) secondaries
    // and is omitted from the server's response when there are none.
    auto passives = res.view()["passives"];
    if (passives && passives.type() == bsoncxx::type::k_array) {
        bsoncxx::array::view passives_view = passives.get_array();
        for (auto member : passives_view) {
            MongodDescription memberDesc;
            memberDesc.mongodUri = nameToUri(std::string(member.get_string().value));
            if (configSetNames.find(setName) != configSetNames.end()) {
                memberDesc.clusterType = ClusterType::configSvrMember;
            } else {
                memberDesc.clusterType = ClusterType::replSetMember;
            }
            desc->nodes.push_back(memberDesc);
        }
    }
    this->_topologyDesc.reset(desc.release());
}

void Topology::findConnectedNodesViaMongos(DBConnection& connection) {
    class ReplSetRetriever : public TopologyVisitor {
    public:
        void onBeforeReplSet(const ReplSetDescription& desc) override {
            replSet = desc;
        }
        ReplSetDescription replSet;
    };

    class ConfigSvrRetriever : public TopologyVisitor {
    public:
        void onBeforeConfigSvr(const ConfigSvrDescription& desc) override {
            configSvr = desc;
        }
        ConfigSvrDescription configSvr;
    };

    mongocxx::uri baseUri(_factory.makeUri());

    std::unique_ptr<ShardedDescription> desc = std::make_unique<ShardedDescription>();
    ReplSetRetriever replSetRetriever;
    ConfigSvrRetriever configSvrRetriever;

    // Config Server
    auto shardMap = connection.runAdminCommand("getShardMap");
    std::string configServerConn(shardMap.view()["map"]["config"].get_string().value);
    auto configConnection = connection.makePeer(nameToUri(configServerConn));
    Topology configTopology(*configConnection);
    configTopology.accept(configSvrRetriever);
    desc->configsvr = configSvrRetriever.configSvr;

    // Shards
    auto shardListRes = connection.runAdminCommand("listShards");
    bsoncxx::array::view shards = shardListRes.view()["shards"].get_array();
    for (auto shard : shards) {
        std::string shardConn(shard["host"].get_string().value);
        auto shardConnection = connection.makePeer(nameToUri(shardConn));
        Topology shardTopology(*shardConnection);
        shardTopology.accept(replSetRetriever);
        desc->shards.push_back(replSetRetriever.replSet);
    }

    // Mongos
    for (auto host : baseUri.hosts()) {
        std::string hostName = host.name + ":" + std::to_string(host.port);
        MongosDescription mongosDesc;
        mongosDesc.mongosUri = nameToUri(hostName);
        desc->mongoses.push_back(mongosDesc);
    }

    this->_topologyDesc.reset(desc.release());
}

void Topology::update(DBConnection& connection) {

    bool isMongos = false;
    auto res = connection.runAdminCommand("isMaster");
    auto msg = res.view()["msg"];
    if (msg && msg.type() == bsoncxx::type::k_utf8) {
        isMongos = msg.get_string().value == "isdbgrid";
    }

    if (isMongos) {
        findConnectedNodesViaMongos(connection);
    } else {
        computeDataMemberConnectionStrings(connection);
    }
}

void ToJsonVisitor::onBeforeTopology(const TopologyDescription&) {
    _result.str("");
    _result.clear();
}

void ToJsonVisitor::onStandaloneMongod(const MongodDescription& desc) {
    _result << "{standaloneMongodUri: " << desc.mongodUri << "}";
}
void ToJsonVisitor::onReplSetMongod(const MongodDescription& desc) {
    _result << "{replSetMemberMongodUri: " << desc.mongodUri << "}";
}
void ToJsonVisitor::onConfigSvrMongod(const MongodDescription& desc) {
    _result << "{configSvrMemberMongodUri: " << desc.mongodUri << "}";
}
void ToJsonVisitor::onBetweenMongods(const ReplSetDescription&) {
    _result << ", ";
}

void ToJsonVisitor::onMongos(const MongosDescription& desc) {
    _result << "{mongosUri: " << desc.mongosUri << "}";
}
void ToJsonVisitor::onBetweenMongoses(const ShardedDescription&) {
    _result << ", ";
}

void ToJsonVisitor::onBeforeReplSet(const ReplSetDescription& desc) {
    _result << "{primaryUri: " << desc.primaryUri << ", nodes: [";
}
void ToJsonVisitor::onAfterReplSet(const ReplSetDescription&) {
    _result << "]}";
}

void ToJsonVisitor::onBeforeShardedCluster(const ShardedDescription&) {
    _result << "{";
}
void ToJsonVisitor::onAfterShardedCluster(const ShardedDescription&) {
    _result << "}";
}

void ToJsonVisitor::onBeforeShards(const ShardedDescription&) {
    _result << " shards: [";
}
void ToJsonVisitor::onBetweenShards(const ShardedDescription&) {
    _result << ", ";
}
void ToJsonVisitor::onAfterShards(const ShardedDescription&) {
    _result << "], ";
}

void ToJsonVisitor::onBeforeMongoses(const ShardedDescription&) {
    _result << " mongoses: [";
}
void ToJsonVisitor::onAfterMongoses(const ShardedDescription&) {
    _result << "]";
}

void ToJsonVisitor::onBeforeConfigSvr(const ConfigSvrDescription& desc){
    _result << "configsvr: {primaryUri: " << desc.primaryUri << ", nodes: [";
}
void ToJsonVisitor::onAfterConfigSvr(const ConfigSvrDescription&){
    _result << "]}";
}

std::string ToJsonVisitor::str() {
    return _result.str();
}


}  // namespace genny::v1
