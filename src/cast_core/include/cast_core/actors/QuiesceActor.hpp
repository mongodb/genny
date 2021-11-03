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

#ifndef HEADER_6F274807_87BE_45A2_AF6F_5B2BF6DBF229_INCLUDED
#define HEADER_6F274807_87BE_45A2_AF6F_5B2BF6DBF229_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * This performs a "whole-cluster" quiesce operation to prevent noise.
 *
 * Examples of use can be found in the workloads/docs/QuiesceActor.yml file.
 *
 * Note: This actor is effectively in beta mode. We expect it to work, but
 * it hasn't been used extensively in production. Please let STM know of any
 * use so we can help monitor its effectiveness.
 *
 * Owner: @mongodb/stm
 */
class QuiesceActor : public Actor {

public:
    explicit QuiesceActor(ActorContext& context);
    ~QuiesceActor() = default;

    void run() override;

    static std::string_view defaultName() {
        return "QuiesceActor";
    }

private:
    mongocxx::pool::entry _client;

    genny::metrics::Operation _totalQuiesces;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_6F274807_87BE_45A2_AF6F_5B2BF6DBF229_INCLUDED
