// Copyright 2023-present MongoDB Inc.
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

#ifndef HEADER_D5EB1FAB_E817_43DE_A126_FCC6995B4C3C_INCLUDED
#define HEADER_D5EB1FAB_E817_43DE_A126_FCC6995B4C3C_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Periodically polls (every 250ms) the stream processor's stats from the
 * mongostream instance (`streams_getStats`) until the expected document count
 * is met. This is needed because stream processing is fully async so we need to
 * rely on metrics emitted from the stream processor itself in order to get proper
 * numbers on throughput.
 *
 * Throughput for a stream processor is measured based on the input rate of
 * the stream processor, so in our case it'll be based on the `inputMessageCount`
 * and `inputMessageBytes` stats in the `streams_getStats` response.
 *
 * ```yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: StreamStatsReporter
 *   Type: StreamStatsReporter
 *   Database: test
 *   Phases:
 *   - Repeat: 1
 *     StreamProcessorName: sp
 *     StreamProcessorId: spid
 *     ExpectedDocumentCount: 1000000
 * ```
 *
 * Owner: @atlas-streams
 */
class StreamStatsReporter : public Actor {
public:
    explicit StreamStatsReporter(ActorContext& context);
    ~StreamStatsReporter() = default;

    void run() override;

    static std::string_view defaultName() {
        return "StreamStatsReporter";
    }

private:
    mongocxx::pool::entry _client;

    // Recorded based on the response of `streams_getStats` from the mongostream
    // instance.
    genny::metrics::Operation _throughput;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    genny::Orchestrator& _orchestrator;
};

}  // namespace genny::actor

#endif  // HEADER_D5EB1FAB_E817_43DE_A126_FCC6995B4C3C_INCLUDED
