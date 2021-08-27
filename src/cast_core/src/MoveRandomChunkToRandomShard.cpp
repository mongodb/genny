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

#include <cast_core/actors/MoveRandomChunkToRandomShard.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

#include <bsoncxx/builder/stream/document.hpp>

namespace genny::actor {

struct MoveRandomChunkToRandomShard::PhaseConfig {
    std::string collectionNamespace;

    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : collectionNamespace{phaseContext["Namespace"].to<std::string>()} {}
};

void MoveRandomChunkToRandomShard::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            try {
                auto&& configDatabase = _client->database("config");
                // Get the collection uuid.
                bsoncxx::document::value collectionFilter = bsoncxx::builder::stream::document()
                    << "_id" << config->collectionNamespace << bsoncxx::builder::stream::finalize;
                auto collectionDocOpt =
                    configDatabase["collections"].find_one(collectionFilter.view());
                // There must be a collection with the provided namespace.
                assert(collectionDocOpt.is_initialized());
                auto uuid = collectionDocOpt.get().view()["uuid"].get_binary();

                // Select a random chunk.
                bsoncxx::document::value uuidDoc = bsoncxx::builder::stream::document()
                    << "uuid" << uuid << bsoncxx::builder::stream::finalize;
                // This is for backward compatibility, before 5.0 chunks were indexed by namespace.
                bsoncxx::document::value nsDoc = bsoncxx::builder::stream::document()
                    << "ns" << config->collectionNamespace << bsoncxx::builder::stream::finalize;
                bsoncxx::document::value chunksFilter = bsoncxx::builder::stream::document()
                    << "$or" << bsoncxx::builder::stream::open_array << uuidDoc.view()
                    << nsDoc.view() << bsoncxx::builder::stream::close_array
                    << bsoncxx::builder::stream::finalize;
                auto numChunks = configDatabase["chunks"].count_documents(chunksFilter.view());
                // The collection must have been sharded and must have at least one chunk;
                std::uniform_int_distribution<int64_t> chunkUniformDistribution{0, numChunks - 1};
                mongocxx::options::find chunkFindOptions;
                auto chunkSort = bsoncxx::builder::stream::document()
                    << "lastmod" << 1 << bsoncxx::builder::stream::finalize;
                chunkFindOptions.sort(chunkSort.view());
                chunkFindOptions.skip(chunkUniformDistribution(_rng));
                chunkFindOptions.limit(1);
                auto chunkProjection = bsoncxx::builder::stream::document()
                    << "history" << false << bsoncxx::builder::stream::finalize;
                chunkFindOptions.projection(chunkProjection.view());
                auto chunkCursor =
                    configDatabase["chunks"].find(chunksFilter.view(), chunkFindOptions);
                assert(chunkCursor.begin() != chunkCursor.end());
                bsoncxx::v_noabi::document::view chunk = *chunkCursor.begin();

                // Find a destination shard different to the source.
                bsoncxx::document::value notEqualDoc = bsoncxx::builder::stream::document()
                    << "$ne" << chunk["shard"].get_utf8() << bsoncxx::builder::stream::finalize;
                bsoncxx::document::value shardFilter = bsoncxx::builder::stream::document()
                    << "_id" << notEqualDoc.view() << bsoncxx::builder::stream::finalize;
                auto numShards = configDatabase["shards"].count_documents(shardFilter.view());
                std::uniform_int_distribution<int64_t> shardUniformDistribution{0, numShards - 1};
                mongocxx::options::find shardFindOptions;
                auto shardSort = bsoncxx::builder::stream::document()
                    << "_id" << 1 << bsoncxx::builder::stream::finalize;
                shardFindOptions.sort(shardSort.view());
                shardFindOptions.skip(shardUniformDistribution(_rng));
                shardFindOptions.limit(1);
                auto shardCursor =
                    configDatabase["shards"].find(shardFilter.view(), shardFindOptions);
                // There must be at least 1 shard, which will be the destination.
                assert(shardCursor.begin() != shardCursor.end());
                bsoncxx::v_noabi::document::view shard = *shardCursor.begin();

                bsoncxx::document::value moveChunkCmd = bsoncxx::builder::stream::document{}
                    << "moveChunk" << config->collectionNamespace << "bounds"
                    << bsoncxx::builder::stream::open_array << chunk["min"].get_value()
                    << chunk["max"].get_value() << bsoncxx::builder::stream::close_array << "to"
                    << shard["_id"].get_utf8() << bsoncxx::builder::stream::finalize;
                // This is only for better readability when logging.
                auto bounds = bsoncxx::builder::stream::document()
                    << "bounds" << bsoncxx::builder::stream::open_array << chunk["min"].get_value()
                    << chunk["max"].get_value() << bsoncxx::builder::stream::close_array
                    << bsoncxx::builder::stream::finalize;
                BOOST_LOG_TRIVIAL(info) << "MoveChunkToRandomShardActor moving chunk "
                                        << bsoncxx::to_json(bounds.view())
                                        << " from: " << chunk["shard"].get_utf8().value.to_string()
                                        << " to: " << shard["_id"].get_utf8().value.to_string();
                _client->database("admin").run_command(moveChunkCmd.view());
            } catch (mongocxx::operation_exception& e) {
                BOOST_THROW_EXCEPTION(MongoException(
                    e,
                    (bsoncxx::builder::stream::document() << bsoncxx::builder::stream::finalize)
                        .view()));
            }
        }
    }
}

MoveRandomChunkToRandomShard::MoveRandomChunkToRandomShard(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _loop{context, MoveRandomChunkToRandomShard::id()},
      _rng{context.workload().getRNGForThread(MoveRandomChunkToRandomShard::id())} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
auto registerMoveRandomChunkToRandomShard = Cast::registerDefault<MoveRandomChunkToRandomShard>();
}  // namespace
}  // namespace genny::actor
