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

#ifndef HEADER_5C95E113_3356_448E_9BB9_5997F62D40FE_INCLUDED
#define HEADER_5C95E113_3356_448E_9BB9_5997F62D40FE_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * MultiCollectionInsert is an actor that performs updates across parameterizable number of
 * collections. Inserts are performed in a loop using `PhaseLoop` and each iteration picks a
 * random collection to insert. The actor records the latency of each update, and the total number
 * of documents updated.
 *
 * See  src/workloads/docs/MultiCollectionInsert.yml for some examples.
 * Owner: product-perf
 */
class MultiCollectionInsert : public Actor {
public:
    explicit MultiCollectionInsert(ActorContext& context);
    ~MultiCollectionInsert() = default;

    static std::string_view defaultName() {
        return "MultiCollectionInsert";
    }
    void run() override;


private:
    /** @private */
    struct PhaseConfig;
    genny::DefaultRandom _rng;

    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_5C95E113_3356_448E_9BB9_5997F62D40FE_INCLUDED
