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

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/topology.hpp>
#include <testlib/helpers.hpp>

using namespace genny;
using namespace std;


TEST_CASE("Topology visitor traverses nodes correctly") {

    class TestVisitor : public TopologyVisitor {
    public:
        virtual void visitMongodDescription(const MongodDescription& desc) { names.push_back(desc.mongodUri); }
        // Mongos falls back to default nop-visit.
        //virtual void visitMongosDescriptionPre(const MongosDescription& desc) {}
        // Same with replset
        //virtual void visitReplSetDescriptionPre(const ReplSetDescription& desc) { names.push_back("visitedPrimary"); }
        virtual void visitShardedDescriptionPre(const ShardedDescription& desc) { names.push_back("visitedShard"); }
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
    MongosDescription mongosDesc;
    mongosDesc.mongosUri = "testMongos";
    shardedCluster.mongoses.push_back(mongosDesc);

    std::vector<std::string> expected{"testConfigPrimaryUri", "testSet0node0", "testSet0node1", "testSet1node0", "testSet1node1", "visitedShard"};
    TestVisitor visitor;
    shardedCluster.accept(visitor);
    REQUIRE(visitor.names.size() == expected.size());
    for (auto name : visitor.names) {
        REQUIRE(std::count(expected.begin(), expected.end(), name));
    }
};

TEST_CASE("Topology maps the cluster correctly") {
    using bsoncxx::builder::stream::array;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::open_document;

    SECTION("Topology correctly maps a standalone") {
        class MockStandaloneService : public DBService {

            ServiceUri uri() { return "testUri"; }

            bsoncxx::document::value runAdminCommand(std::string command) {
                if (command == "isMaster") {
                    auto doc = document{};
                    return doc << "junkKey" << "junkValue" << finalize;
                }
                return document{} << "unplannedKey" << "unplannedValue" << finalize;
            }

            std::unique_ptr<DBService> makePeer(ServiceUri uri) {
                return std::make_unique<MockStandaloneService>();
            }
        };
        MockStandaloneService service;
        Topology topology(service);

        ToJsonVisitor visitor;
        topology.accept(visitor);
        auto expected = "{mongodUri: testUri}";
        REQUIRE(expected == visitor.str());
    }

    SECTION("Topology correctly maps a replica set") {
        class MockReplService : public DBService {

            ServiceUri uri() { return "testPrimaryUriNeverUsedHere"; }

            bsoncxx::document::value runAdminCommand(std::string command) {
                if (command == "isMaster") {
                    auto doc = document{};
                    doc << "setName" << "testSetName";
                    doc << "primary" << "testPrimaryHost:testPrimaryPort";
                    doc << "hosts"
                        << open_array << "testPrimaryHost:testPrimaryPort" << "host2:port2"
                        << "host3:port3" << close_array;
                    return doc << finalize;

                }
                return document{} << "unplannedKey" << "unplannedValue" << finalize;
            }

            std::unique_ptr<DBService> makePeer(ServiceUri uri) {
                return std::make_unique<MockReplService>();
            }
        };
        MockReplService service;
        Topology topology(service);

        ToJsonVisitor visitor;
        topology.accept(visitor);

        stringstream expected;
        expected << "{primaryUri: mongodb://testPrimaryHost:testPrimaryPort/?appName=Genny, "
            << "nodes: [{mongodUri: mongodb://testPrimaryHost:testPrimaryPort/?appName=Genny}, "
                    << "{mongodUri: mongodb://host2:port2/?appName=Genny}, "
                    << "{mongodUri: mongodb://host3:port3/?appName=Genny}]}";

        REQUIRE(expected.str() == visitor.str());
    }

    SECTION("Topology correctly maps a sharded cluster") {
        class MockConfigService : public DBService {

            ServiceUri uri() { return "testConfigUriNeverUsedHere"; }

            bsoncxx::document::value runAdminCommand(std::string command) {
                if (command == "isMaster") {
                    auto doc = document{};
                    doc << "setName" << "configSet";
                    doc << "primary" << "testConfigHost:testConfigPort";
                    doc << "hosts"
                        << open_array << "testConfigHost:testConfigPort" << close_array;
                    return doc << finalize;

                }
                return document{} << "unplannedKey" << "unplannedValue" << finalize;
            }

            std::unique_ptr<DBService> makePeer(ServiceUri uri) {
                return std::make_unique<MockConfigService>();
            }
        };
        class MockShardService : public DBService {

            ServiceUri uri() { return "testShardUriNeverUsedHere"; }

            bsoncxx::document::value runAdminCommand(std::string command) {
                if (command == "isMaster") {
                    auto doc = document{};
                    doc << "setName" << "shard1";
                    doc << "primary" << "shardNode1:shardPort1";
                    doc << "hosts"
                        << open_array << "shardNode1:shardPort1" << "shardNode2:shardPort2" << close_array;
                    return doc << finalize;

                }
                return document{} << "unplannedKey" << "unplannedValue" << finalize;
            }

            std::unique_ptr<DBService> makePeer(ServiceUri uri) {
                return std::make_unique<MockConfigService>();
            }
        };

        class MockShardedClusterService : public DBService {

            ServiceUri uri() { return "mongodb://testMongosUri:11111/?appName=Genny"; }

            bsoncxx::document::value runAdminCommand(std::string command) {

                if (command == "isMaster") {
                    return document{} << "msg" << "isdbgrid" << finalize;

                }
                if (command == "getShardMap") {
                    auto doc = document{};
                    doc << "map" << open_document
                        << "config" << "configSvr/configHost:configPort"
                        << close_document;
                    return doc << finalize;
                }
                if (command == "listShards") {
                    auto doc = document{};
                    doc << "shards" << open_array
                        << open_document << "host"
                        << "shard1/shardNode1:shardPort1,shardNode2:shardPort2" << close_document
                        << close_array;
                    return doc << finalize;
                }
                return document{} << "unplannedKey" << "unplannedValue" << finalize;
            }

            std::unique_ptr<DBService> makePeer(ServiceUri uri) {
                if (uri == "mongodb://configHost:configPort/?appName=Genny") {
                    return std::make_unique<MockConfigService>();
                }
                if (uri == "mongodb://shardNode1:shardPort1,shardNode2:shardPort2/?appName=Genny") {
                    return std::make_unique<MockShardService>();
                }
                return std::make_unique<MockShardedClusterService>();
            }
        };
        MockShardedClusterService service;
        Topology topology(service);

        ToJsonVisitor visitor;
        topology.accept(visitor);

        stringstream expected;
        expected << "{configsvr: {primaryUri: mongodb://testConfigHost:testConfigPort/?appName=Genny, "
                      << "nodes: [{mongodUri: mongodb://testConfigHost:testConfigPort/?appName=Genny}]} "
                 << "shards: [{primaryUri: mongodb://shardNode1:shardPort1/?appName=Genny, "
                            << "nodes: [{mongodUri: mongodb://shardNode1:shardPort1/?appName=Genny}, "
                            << "{mongodUri: mongodb://shardNode2:shardPort2/?appName=Genny}]}],  "
                 << "mongoses: [{mongosUri: mongodb://testmongosuri:11111/?appName=Genny}]}";

        REQUIRE(expected.str() == visitor.str());
    }

};
