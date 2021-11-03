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

#ifndef HEADER_6F5BD1CA_E5CC_45F6_88FE_C823F3F2B2EA_INCLUDED
#define HEADER_6F5BD1CA_E5CC_45F6_88FE_C823F3F2B2EA_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <metrics/operation.hpp>
#include <value_generators/DefaultRandom.hpp>

namespace genny::actor {

/**
 * Latencies of a read-write transaction with different read concern and write concern.
 *
 * Owner: @mongodb/product-perf
 */
class CommitLatency : public Actor {

public:
    explicit CommitLatency(ActorContext& context, ActorId id);
    ~CommitLatency() = default;

    static std::string_view defaultName() {
        return "CommitLatency";
    }

    void run() override;

private:
    mongocxx::pool::entry _client;
    genny::DefaultRandom& _rng;

    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_6F5BD1CA_E5CC_45F6_88FE_C823F3F2B2EA_INCLUDED
