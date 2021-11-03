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

#ifndef HEADER_4EFEA0DE_80BA_4D8F_A9B5_E874147B1181_INCLUDED
#define HEADER_4EFEA0DE_80BA_4D8F_A9B5_E874147B1181_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Moves a random chunk to a randomly chosen shard.
 *
 * ```yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: MoveChunk
 *   Type: MoveRandomChunkToRandomShard
 *   Phases:
 *   - Namespace: test.coll
 *
 * ```
 *
 * Owner: @mongodb/sharding
 */
class MoveRandomChunkToRandomShard : public Actor {
public:
    explicit MoveRandomChunkToRandomShard(ActorContext& context);
    ~MoveRandomChunkToRandomShard() = default;

    void run() override;

    static std::string_view defaultName() {
        return "MoveRandomChunkToRandomShard";
    }

private:
    mongocxx::pool::entry _client;
    genny::DefaultRandom& _rng;

    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_4EFEA0DE_80BA_4D8F_A9B5_E874147B1181_INCLUDED
