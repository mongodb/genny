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

#include <string>
#include <optional>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/topology.hpp>

namespace genny {

TopologyVisitor::~TopologyVisitor() {}

TopologyDescription::~TopologyDescription() {}

void MongodDescription::accept(TopologyVisitor& v) { 
    v.onMongod(*this);
}

void MongosDescription::accept(TopologyVisitor& v) { v.onMongos(*this); }

void ReplSetDescription::accept(TopologyVisitor& v) { 
    v.onBeforeReplSet(*this); 
    for (int i = 0; i < nodes.size() - 1; i++) {
        nodes[i].accept(v);
        v.onBetweenMongods(*this);
    }
    nodes[nodes.size() - 1].accept(v);
    v.onAfterReplSet(*this); 
}

void ShardedDescription::accept(TopologyVisitor& v) { 
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

std::string Topology::nameToUri(const std::string& name) {
    std::set<std::string> nameSet;
    // Hostnames returned from some commands have the form configRepl/hostname:port
    auto strippedName = name.substr(name.find('/') + 1, std::string::npos);
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
        desc->mongodUri = connection.uri();
        this->_topologyDesc.reset(desc.release());
        return;
    }

    auto primary = res.view()["primary"];

    std::unique_ptr<ReplSetDescription> desc = std::make_unique<ReplSetDescription>();
    desc->primaryUri = nameToUri(std::string(primary.get_utf8().value));

    auto hosts = res.view()["hosts"];
    if (hosts && hosts.type() == bsoncxx::type::k_array) {
        bsoncxx::array::view hosts_view = hosts.get_array();
        for (auto member : hosts_view) {
            MongodDescription memberDesc;
            memberDesc.mongodUri = nameToUri(std::string(member.get_utf8().value));
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
            memberDesc.mongodUri = std::string(member.get_utf8().value);
            desc->nodes.push_back(memberDesc);
        }
    }

    this->_topologyDesc.reset(desc.release());
}

void Topology::findConnectedNodesViaMongos(DBConnection& connection) {
    class ReplSetRetriever : public TopologyVisitor {
    public:
        void onBeforeReplSet(const ReplSetDescription& desc) {
            replSet = desc; 
        }
        ReplSetDescription replSet;
    };

    mongocxx::uri baseUri(_factory.makeUri());

    std::unique_ptr<ShardedDescription> desc = std::make_unique<ShardedDescription>();
    ReplSetRetriever retriever;

    // Config Server
    auto shardMap = connection.runAdminCommand("getShardMap");
    std::string configServerConn(shardMap.view()["map"]["config"].get_utf8().value);
    auto configConnection = connection.makePeer(nameToUri(configServerConn));
    Topology configTopology(*configConnection);
    configTopology.accept(retriever);
    desc->configsvr = retriever.replSet;
    desc->configsvr.configsvr = true;

    // Shards
    auto shardListRes = connection.runAdminCommand("listShards");
    bsoncxx::array::view shards = shardListRes.view()["shards"].get_array();
    for (auto shard : shards) {
        std::string shardConn(shard["host"].get_utf8().value);
        auto shardConnection = connection.makePeer(nameToUri(shardConn));
        Topology shardTopology(*shardConnection);
        shardTopology.accept(retriever);
        desc->shards.push_back(retriever.replSet);
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
        isMongos = msg.get_utf8().value == "isdbgrid";
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

void ToJsonVisitor::onMongod(const MongodDescription& desc) { _result << "{mongodUri: " << desc.mongodUri << "}"; }
void ToJsonVisitor::onBetweenMongods(const ReplSetDescription& desc) { _result << ", "; }

void ToJsonVisitor::onMongos(const MongosDescription& desc) { _result << "{mongosUri: " << desc.mongosUri << "}"; }
void ToJsonVisitor::onBetweenMongoses(const ReplSetDescription& desc) { _result << ", "; }

void ToJsonVisitor::onBeforeReplSet(const ReplSetDescription& desc) {
    if (desc.configsvr) {
        _result << "configsvr: ";
    }
    _result << "{primaryUri: " << desc.primaryUri << ", nodes: [";
}
void ToJsonVisitor::onAfterReplSet(const ReplSetDescription& desc) { _result << "]}"; }

void ToJsonVisitor::onBeforeSharded(const ShardedDescription&) { _result << "{"; }
void ToJsonVisitor::onAfterSharded(const ShardedDescription&) { _result << "}"; }

void ToJsonVisitor::onBeforeShards(const ShardedDescription&) { _result << " shards: ["; }
void ToJsonVisitor::onBetweenShards(const ShardedDescription&) { _result << ", "; }
void ToJsonVisitor::onAfterShards(const ShardedDescription&) { _result << "], "; }

void ToJsonVisitor::onBeforeMongoses(const ShardedDescription&) { _result << " mongoses: ["; }
void ToJsonVisitor::onBetweenMongoses(const ShardedDescription&) { _result << ", "; }
void ToJsonVisitor::onAfterMongoses(const ShardedDescription&) { _result << "]"; }

std::string ToJsonVisitor::str() { return _result.str(); }


}  // namespace genny
