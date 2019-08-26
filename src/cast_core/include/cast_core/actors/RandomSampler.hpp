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

#ifndef HEADER_1C384BE6_02A0_4C40_A866_B5122009F396_INCLUDED
#define HEADER_1C384BE6_02A0_4C40_A866_B5122009F396_INCLUDED

#include <mongocxx/pool.hpp>
#include <string_view>

#include <cast_core/actors/CollectionScanner.hpp>
#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * This actor will sample 10 documents from the collections it is tasked with
 * continuously.
 *
 * Example yaml can be found at src/workloads/docs/RandomSampler.yml
 *
 * Owner: Storage Engines
 */
class RandomSampler : public Actor {
    // Used to assign each RandomSampler instance an id starting at 0.
    // The genny::Acttor::id() field is monotonically increasing across all Actors
    // of all types.
    struct ActorCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

public:
    explicit RandomSampler(ActorContext& context);
    ~RandomSampler() = default;
    void run() override;
    static std::string_view defaultName() {
        return "RandomSampler";
    }

private:
    mongocxx::pool::entry _client;
    /** @private */
    struct PhaseConfig;
    DefaultRandom& _random;
    int _index;
    PhaseLoop<PhaseConfig> _loop;
    CollectionScanner::RunningActorCounter& _activeCollectionScannerInstances;
};

}  // namespace genny::actor

#endif  // HEADER_1C384BE6_02A0_4C40_A866_B5122009F396_INCLUDED
