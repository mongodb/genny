// Copyright 2022-present MongoDB Inc.
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

#ifndef HEADER_25AE844D_6E55_42EB_9E93_56C7CB727F54_INCLUDED
#define HEADER_25AE844D_6E55_42EB_9E93_56C7CB727F54_INCLUDED

#include <string_view>
#include <string>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 *
 * Owner: 10gen/query
 */
class ExternalScriptRunner : public Actor {


public:

    explicit ExternalScriptRunner(ActorContext& context);
    ~ExternalScriptRunner() = default;

    void run() override;

    static std::string_view defaultName() {
        return "ExternalScriptRunner";
    }

private:

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    std::string _command;
    std::unordered_map<std::string, std::string> _environmentVariables;
};

}  // namespace genny::actor

#endif  // HEADER_25AE844D_6E55_42EB_9E93_56C7CB727F54_INCLUDED
