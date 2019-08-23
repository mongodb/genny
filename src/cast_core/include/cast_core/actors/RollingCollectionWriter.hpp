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

#ifndef HEADER_34CFFB35_C0EB_42FA_ACF2_91A7E6556841_INCLUDED
#define HEADER_34CFFB35_C0EB_42FA_ACF2_91A7E6556841_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>
#include <cast_core/actors/RollingCollectionManager.hpp>
#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Writes into the latest collection created by the rolling collection
 * manager.
 *
 * For use example see: src/workloads/docs/RollingCollectionManager.yml
 *
 * Owner: Storage Engines
 */
class RollingCollectionWriter : public Actor {
public:
    explicit RollingCollectionWriter(ActorContext& context);
    ~RollingCollectionWriter() = default;
    void run() override;
    static std::string_view defaultName() {
        return "RollingCollectionWriter";
    }

private:
    RollingCollectionManager::RollingCollectionNames& _rollingCollectionNames;
    mongocxx::pool::entry _client;
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};
}  // namespace genny::actor

#endif  // HEADER_34CFFB35_C0EB_42FA_ACF2_91A7E6556841_INCLUDED
