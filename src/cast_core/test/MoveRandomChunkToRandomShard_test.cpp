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

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture,
                 "MoveRandomChunkToRandomShard successfully connects to a MongoDB instance.",
                 "[sharded][MoveRandomChunkToRandomShard]") {

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: CreateShardedCollection
        Type: AdminCommand
        Threads: 1
        Phases:
        - Repeat: 1
            Database: admin
            Operations:
            - OperationMetricsName: EnableSharding
            OperationName: AdminCommand
            OperationCommand:
                enableSharding: test
            - OperationMetricsName: ShardCollection
            OperationName: AdminCommand
            OperationCommand:
                shardCollection: test.collection0
                key: {Key: 1}
        - {Nop: true}

        - Name: MoveRandomChunkToRandomShard
          Type: MoveRandomChunkToRandomShard
          Phases:
          - {Nop: true}
          - Repeat: 1
            Thread: 1
            Namespace: test.collection0
    )",
                                  __FILE__);


    SECTION("Moves a random chunk to a random shard.") {
        try {
            dropAllDatabases();
            auto db = client.database("test");

            genny::ActorHelper ah(nodes.root(), 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
