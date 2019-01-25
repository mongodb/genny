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

#ifndef HEADER_F86B8CA3_F0C0_4973_9FC8_3875A76D7610
#define HEADER_F86B8CA3_F0C0_4973_9FC8_3875A76D7610

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <metrics/metrics.hpp>
#include <value_generators/value_generators.hpp>

namespace genny::actor {

/**
 * MultiCollectionQuery is an actor that performs updates across parameterizable number of
 * collections. Updates are performed in a loop using `PhaseLoop` and each iteration picks a
 * random collection to update. The actor records the latency of each update, and the total number
 * of documents updated.
 */
class MultiCollectionQuery : public Actor {

public:
    explicit MultiCollectionQuery(ActorContext& context);
    ~MultiCollectionQuery() override = default;

    static std::string_view defaultName() {
        return "MultiCollectionQuery";
    }
    void run() override;

private:
    /** @private */
    struct PhaseConfig;
    genny::DefaultRandom _rng;
    metrics::Timer _queryTimer;
    metrics::Counter _documentCount;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_F86B8CA3_F0C0_4973_9FC8_3875A76D7610
