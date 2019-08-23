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

#ifndef HEADER_77FF92CF_C4C9_48A6_AC48_7920C4CFA699_INCLUDED
#define HEADER_77FF92CF_C4C9_48A6_AC48_7920C4CFA699_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>
#include <cast_core/actors/RollingCollectionManager.hpp>
#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Reads collections created by the rolling collection manager.
 * It will chose which collection to read from based off a linear
 * distribution configurable using the Distribution configuration option.
 *
 * It can read from indexes if desired specified with the Filter option.
 *
 * For use example see: src/workloads/docs/RollingCollectionManager.yml
 *
 * Owner: Storage Engines
 */
class RollingCollectionReader : public Actor {
public:
    explicit RollingCollectionReader(ActorContext& context);
    ~RollingCollectionReader() = default;
    void run() override;
    static std::string_view defaultName() {
        return "RollingCollectionReader";
    }

private:
    RollingCollectionManager::RollingCollectionNames& _rollingCollectionNames;
    mongocxx::pool::entry _client;
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    DefaultRandom& _random;
    int64_t _collectionWindowSize;
};

}  // namespace genny::actor

#endif  // HEADER_77FF92CF_C4C9_48A6_AC48_7920C4CFA699_INCLUDED
