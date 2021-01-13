// Copyright 2019-present MongoDB Inc.
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

#include <algorithm>
#include <gennylib/topology.hpp>

#include <testlib/helpers.hpp>

using namespace genny;
using namespace std;


TEST_CASE("Topology descriptions get nodes correctly") {

    class TestVisitor : public TopologyVisitor {
    public:
        virtual void visitMongodDescription(const MongodDescription& desc) { names.push_back(desc.mongodUri); }
        // Mongos falls back to default nop-visit.
        //virtual void visitMongosDescription(const MongosDescription& desc) {}
        // Same with replset
        //virtual void visitReplSetDescription(const ReplSetDescription& desc) { names.push_back("visitedPrimary"); }
        virtual void visitShardedDescription(const ShardedDescription& desc) { names.push_back("visitedShard"); }
        std::vector<std::string> names;
    };

    ShardedDescription shardedCluster;

    // Config server
    shardedCluster.configsvr.primaryUri = "testConfigPrimaryUri";
    shardedCluster.configsvr.nodes.emplace_back();
    shardedCluster.configsvr.nodes[0].mongodUri = "testConfigPrimaryUri";

    // Shards
    for (int i = 0; i < 2; i++) {
        ReplSetDescription replSet;
        replSet.primaryUri = "testSet" + std::to_string(i) + "node0";
        for (int j = 0; j < 2; j++) {
            auto nodeName = "testSet" + std::to_string(i) + "node" + std::to_string(j);
            replSet.nodes.emplace_back();
            replSet.nodes[j].mongodUri = nodeName;
        }
        shardedCluster.shards.push_back(replSet);
    }

    // Mongos
    shardedCluster.mongos.mongosUri = "testMongos";

    std::vector<std::string> expected{"testConfigPrimaryUri", "testSet0node0", "testSet0node1", "testSet1node0", "testSet1node1", "visitedShard"};
    TestVisitor visitor;
    shardedCluster.accept(visitor);
    REQUIRE(visitor.names.size() == expected.size());
    for (auto name : visitor.names) {
        REQUIRE(std::count(expected.begin(), expected.end(), name));
    }
};
