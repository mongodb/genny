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

#ifndef HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
#define HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED

#include <mongocxx/client_session.hpp>

#include <gennylib/PhaseLoop.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * A "Proof of Concept" dumb Actor that we use
 * to smoke-test framework features.
 *
 * Owner: STM
 */
class HelloWorld : public genny::Actor {

    /**
     * Example of shared state.
     * @see genny::WorkloadContext::ShareableState
     */
    struct HelloWorldCounter : genny::WorkloadContext::ShareableState<std::atomic_int> {};

public:
    /**
     * Construct a HelloWorld.
     * @param context represents the `Actor` block.
     */
    explicit HelloWorld(ActorContext& context);

    /** Destruct */
    ~HelloWorld() override = default;

    /** @return name to use for metrics etc. */
    static std::string_view defaultName() {
        return "HelloWorld";
    }

    void run() override;

private:
    /** record data about each iteration */
    metrics::Operation _operation;

    /** constructed from each `Phase:` block in the `Actor:` block */
    struct PhaseConfig;
    /** loops over each Phase and handles Duration/Repeat/Rate */
    PhaseLoop<PhaseConfig> _loop;
    /** example of sharing data. @see HelloWorldCounter */
    HelloWorldCounter& _hwCounter;
};

}  // namespace genny::actor

#endif  // HEADER_FF3E897B_C747_468B_AAAC_EA6421DB0902_INCLUDED
