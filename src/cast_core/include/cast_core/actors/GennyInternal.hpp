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

#ifndef HEADER_255CB4E5_1FC3_431F_8E2F_7251A3A22B94_INCLUDED
#define HEADER_255CB4E5_1FC3_431F_8E2F_7251A3A22B94_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Actor for tracking and reporting Genny internal state.
 *
 * Currently intended to be run in a single-threaded manner. This actor
 * is automatically instantiated by Genny's preprocessor by default.
 *
 * Reports:
 *   GennyInternal.Phase - Records an event at the end of each phase, with a duration the length of the phase.
 *
 * Owner: 10gen/dev-prod-tips
 */
class GennyInternal : public Actor {

public:
    explicit GennyInternal(ActorContext& context);
    ~GennyInternal() = default;

    void run() override;

    static std::string_view defaultName() {
        return "GennyInternal";
    }

private:
    genny::metrics::Operation _phaseOp;

    /** @private */
    Orchestrator& _orchestrator;
};

}  // namespace genny::actor

#endif  // HEADER_255CB4E5_1FC3_431F_8E2F_7251A3A22B94_INCLUDED
