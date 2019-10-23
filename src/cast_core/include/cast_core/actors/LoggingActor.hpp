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

#ifndef HEADER_F11FE181_61A3_460F_859D_ED9D60954E50_INCLUDED
#define HEADER_F11FE181_61A3_460F_859D_ED9D60954E50_INCLUDED

#include <string_view>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * Refer to workloads/docs/LoggingActor.yml for configuration examples.
 * Owner: STM Team
 */
class LoggingActor : public Actor {
public:
    explicit LoggingActor(ActorContext& context);
    ~LoggingActor() override = default;
    void run() override;
    static std::string_view defaultName() {
        return "LoggingActor";
    }

private:
    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_F11FE181_61A3_460F_859D_ED9D60954E50_INCLUDED
