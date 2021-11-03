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

#ifndef HEADER_ADE63106_C23B_4762_88FB_C5240A7170F9_INCLUDED
#define HEADER_ADE63106_C23B_4762_88FB_C5240A7170F9_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * A Nop Actor used to measure overhead of Genny internals, including looping and metrics
 * collection.
 * The only thing this actor does is start then immediately stop recording a dummy metric.
 *
 * Owner: stm
 */
class NopMetrics : public Actor {

public:
    explicit NopMetrics(ActorContext& context, ActorId id);
    ~NopMetrics() = default;

    void run() override;

    static std::string_view defaultName() {
        return "NopMetrics";
    }

private:
    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_ADE63106_C23B_4762_88FB_C5240A7170F9_INCLUDED
