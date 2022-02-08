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

#ifndef HEADER_F39B7C7C_1002_4060_AAEE_F379AEE7AB3C_INCLUDED
#define HEADER_F39B7C7C_1002_4060_AAEE_F379AEE7AB3C_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

class SampleHttpClient : public Actor {

public:
    explicit SampleHttpClient(ActorContext& context);
    ~SampleHttpClient() = default;

    void run() override;

    static std::string_view defaultName() {
        return "SampleHttpClient";
    }

private:
    mongocxx::pool::entry _client;

    genny::metrics::Operation _totalRequests;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_F39B7C7C_1002_4060_AAEE_F379AEE7AB3C_INCLUDED
