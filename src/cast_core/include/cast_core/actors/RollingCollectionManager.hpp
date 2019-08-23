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

#ifndef HEADER_ED16EA41_82CE_428D_B7D1_94AAEE3AF70C_INCLUDED
#define HEADER_ED16EA41_82CE_428D_B7D1_94AAEE3AF70C_INCLUDED

#include <string_view>
#include <queue>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/**
 * Creates and deletes one collection per iteration, indexes are configurable
 * at the top level of the actor, additionally has a setup phase which allows
 * it to create an amount of collections as defined by CollectionCount.
 *
 * The collections created are named r_X. Not to be used with more
 * than one thread.
 *
 * For use example see: src/workloads/docs/RollingCollectionManager.yml
 *
 * Owner: Storage Engines
 */
class RollingCollectionManager : public Actor {
public:
    struct RollingCollectionNames : genny::WorkloadContext::ShareableState<std::deque<std::string>> {};

    explicit RollingCollectionManager(ActorContext& context);
    ~RollingCollectionManager() = default;
    void run() override;

    static std::string_view defaultName() {
        return "RollingCollectionManager";
    }
private:
    mongocxx::pool::entry _client;
    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    std::vector<DocumentGenerator> _indexConfig;
    RollingCollectionNames& _collectionNames;
    int64_t _currentCollectionId;
    int64_t _collectionWindowSize;
};
std::string getRollingCollectionName(int64_t lastId);
}
#endif  // HEADER_ED16EA41_82CE_428D_B7D1_94AAEE3AF70C_INCLUDED
