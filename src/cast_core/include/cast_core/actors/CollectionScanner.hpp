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

#ifndef HEADER_18942B0C_FAD8_4B03_A09D_F27EAAB8C219_INCLUDED
#define HEADER_18942B0C_FAD8_4B03_A09D_F27EAAB8C219_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * This actor will scan all collections it is tasked with.
 *
 * Example yaml can be found at src/workloads/docs/CollectionScanner.yml
 *
 * Owner: Storage Engines
 */
class CollectionScanner : public Actor {
    // Used to assign each CollectionScanner instance an id starting at 0.
    // The genny::Acttor::id() field is monotonically increasing across all Actors
    // of all types.
    struct ActorCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

public:
    // Tracks how many instances of this Actor are currently running.
    struct RunningActorCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

    explicit CollectionScanner(ActorContext& context);
    ~CollectionScanner() = default;
    void run() override;
    static std::string_view defaultName() {
        return "CollectionScanner";
    }
    struct PhaseConfig;

private:
    mongocxx::pool::entry _client;
    genny::metrics::Operation _totalInserts;

    /** @private */
    int _index;
    RunningActorCounter& _runningActorCounter;
    std::string _databaseNames;
    PhaseLoop<PhaseConfig> _loop;
    bool _generateCollectionNames;
    GlobalRateLimiter* _rateLimiter;
};

// Defined in CollectionScanner.cpp but used by CollectionScanner and RandomSampler
std::vector<std::string> distributeCollectionNames(size_t, size_t, ActorId);

}  // namespace genny::actor

#endif  // HEADER_18942B0C_FAD8_4B03_A09D_F27EAAB8C219_INCLUDED
