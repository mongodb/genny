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

#include <queue>
#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <gennylib/parallel.hpp>

namespace genny::actor {
/**
 * This actor provides a rolling collection functionality, it has 4 operations:
 * Setup: Creates an initial set of collections and creates documents within them.
 * Manage: Creates and deletes a collection per iteration.
 * Read: Reads from the set of collection preferencing reading the most recently created collection.
 * Write: Writes to the most recently created collection.
 *
 * The collections created are named rX_timestamp.
 *
 * For use example see: src/workloads/docs/RollingCollections.yml
 *
 * Owner: Storage Engines
 */
class RollingCollections : public Actor {
public:
    struct RollingCollectionNames
        : genny::WorkloadContext::ShareableState<AtomicDeque<std::string>> {};

    explicit RollingCollections(ActorContext& context);
    ~RollingCollections() = default;
    void run() override;

    static std::string_view defaultName() {
        return "RollingCollections";
    }

private:
    mongocxx::pool::entry _client;
    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    RollingCollectionNames& _collectionNames;
};
std::string getRollingCollectionName(int64_t lastId);
}  // namespace genny::actor
#endif  // HEADER_ED16EA41_82CE_428D_B7D1_94AAEE3AF70C_INCLUDED
