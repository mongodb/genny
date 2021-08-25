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
                 "MoveRandomChunkToRandomShard",
                 "[sharded][MoveRandomChunkToRandomShard]") {

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: MoveRandomChunkToRandomShard
          Type: MoveRandomChunkToRandomShard
          Phases:
          - Repeat: 1
            Thread: 1
            Namespace: test.collection0
    )",
                                  __FILE__);


    SECTION("Moves a random chunk to a random shard.") {
        try {
            dropAllDatabases();

            // Enable sharding for database test.
            bsoncxx::document::value enableSardingCmd = bsoncxx::builder::stream::document{}
                << "enableSharding"
                << "test" << bsoncxx::builder::stream::finalize;
            client.database("admin").run_command(enableSardingCmd.view());

            // Shard a collection.
            bsoncxx::document::value shardKey = bsoncxx::builder::stream::document{}
                << "key" << 1 << bsoncxx::builder::stream::finalize;
            bsoncxx::document::value shardCollectionCmd = bsoncxx::builder::stream::document{}
                << "shardCollection"
                << "test.collection0"
                << "key" << shardKey << bsoncxx::builder::stream::finalize;
            client.database("admin").run_command(shardCollectionCmd.view());

            // Get the collection uuid.
            auto&& configDatabase = client.database("config");
            bsoncxx::document::value collectionFilter = bsoncxx::builder::stream::document()
                << "_id"
                << "test.collection0" << bsoncxx::builder::stream::finalize;
            auto collectionDocOpt = configDatabase["collections"].find_one(collectionFilter.view());
            auto uuid = collectionDocOpt.get().view()["uuid"].get_binary();

            // Get the chunk information, the lower bound (for comparison) and the shard id.
            bsoncxx::document::value chunksFilter = bsoncxx::builder::stream::document()
                << "uuid" << uuid << bsoncxx::builder::stream::finalize;
            auto chunkProjection = bsoncxx::builder::stream::document()
                << "history" << false << bsoncxx::builder::stream::finalize;
            mongocxx::options::find chunkFindOptions;
            chunkFindOptions.projection(chunkProjection.view());
            // There is only one chunk, store the initial shard id.
            auto chunkOpt =
                configDatabase["chunks"].find_one(chunksFilter.view(), chunkFindOptions);

            auto initialShardId = chunkOpt.get().view()["shard"].get_utf8().value.to_string();

            // Run the actor.
            genny::ActorHelper ah(nodes.root(), 1, MongoTestFixture::connectionUri().to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            // Compare the chunk shard id after the move.
            auto afterMigrationChunkOpt =
                configDatabase["chunks"].find_one(chunksFilter.view(), chunkFindOptions);

            auto finalShardId =
                afterMigrationChunkOpt.get().view()["shard"].get_utf8().value.to_string();

            REQUIRE(initialShardId != finalShardId);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
