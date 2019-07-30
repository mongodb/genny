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
 * Example yaml can be found under workloads/docs
 *
 * Owner: Storage Engines
 */
class CollectionScanner : public Actor {
    struct ActorCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

public:
    struct RunningActorCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

    static std::vector<std::string> getCollectionNames(size_t collectionCount,
                                                       size_t threadCount,
                                                       ActorId actorId) {
        // We always want a fair division of collections to actors
        std::vector<std::string> collectionNames{};
        if ((threadCount > collectionCount && threadCount % collectionCount != 0) ||
            collectionCount % threadCount != 0) {
            throw std::invalid_argument(
                "Thread count must be mutliple of database collection count");
        }
        int collectionsPerActor = threadCount > collectionCount ? 1 : collectionCount / threadCount;
        int collectionIndexStart = (actorId % collectionCount) * collectionsPerActor;
        int collectionIndexEnd = collectionIndexStart + collectionsPerActor;
        for (int i = collectionIndexStart; i < collectionIndexEnd; ++i) {
            collectionNames.push_back("Collection" + std::to_string(i));
        }
        return collectionNames;
    }

    explicit CollectionScanner(ActorContext& context);
    ~CollectionScanner() = default;
    void run() override;
    static std::string_view defaultName() {
        return "CollectionScanner";
    }

private:
    mongocxx::pool::entry _client;
    genny::metrics::Operation _totalInserts;

    /** @private */
    struct PhaseConfig;
    ActorCounter& _actorCounter;
    RunningActorCounter& _runningActorCounter;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_18942B0C_FAD8_4B03_A09D_F27EAAB8C219_INCLUDED
