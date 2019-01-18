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

#ifndef HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46
#define HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <metrics/metrics.hpp>
#include <value_generators/value_generators.hpp>

namespace genny::actor {

/**
 * MultiCollectionUpdate is an actor that performs updates across parameterizable number of
 * collections. Updates are performed in a loop using `PhaseLoop` and each iteration picks a
 * random collection to update. The actor records the latency of each update, and the total number
 * of documents updated.
 */
class MultiCollectionUpdate : public Actor {

public:
    explicit MultiCollectionUpdate(ActorContext& context);
    ~MultiCollectionUpdate() = default;

    static std::string_view defaultName() {
        return "MultiCollectionUpdate";
    }
    void run() override;

private:
    struct PhaseConfig;
    genny::DefaultRandom _rng;

    metrics::Timer _updateTimer;
    metrics::Counter _updateCount;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_D112CCC3_DF60_434E_A038_5A7AADED0E46
