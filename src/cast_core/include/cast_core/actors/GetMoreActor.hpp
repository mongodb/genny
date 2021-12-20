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

#ifndef HEADER_4ACF7ADF_14B2_4444_81BA_A17636BAEDA1_INCLUDED
#define HEADER_4ACF7ADF_14B2_4444_81BA_A17636BAEDA1_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * An actor for running cursor commands until they are exhausted.
 *
 * Supported server commands which return a cursor include:
 *   - find
 *   - aggregate
 *   - listIndexes
 *
 * Refer to the MongoDB Manual for documentation on their respective command
 * syntaxes.
 *
 * The GetMoreBatchSize parameter is optional. Omitting it will cause the server
 * to use its default batch size for the cursor. The initial batch size is
 * configurable through the InitialCursorCommand using the appropriate command
 * syntax.
 *
 * InitialCursorCommand accepts generators and they will be re-evaluated for
 * each new cursor.
 *
 * ```yaml
 * SchemaVersion: 2018-07-01
 * Actors:
 * - Name:
 *   Type: GetMoreActor
 *   Phases:
 *   - Repeat: 1
 *     Database: mydb
 *     InitialCursorCommand:
 *       find: mycoll
 *       batchSize: 1
 *     GetMoreBatchSize: 2
 * ```
 *
 * See docs/GetMoreActor.yml for additional examples.
 *
 * Owner: @mongodb/sharding
 */
class GetMoreActor : public Actor {
public:
    explicit GetMoreActor(ActorContext& context);
    ~GetMoreActor() = default;

    void run() override;

    static std::string_view defaultName() {
        return "GetMoreActor";
    }

private:
    mongocxx::pool::entry _client;

    metrics::Operation _overallCursor;
    metrics::Operation _initialRequest;
    metrics::Operation _individualGetMore;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;

    static int64_t _runCursorCommand(mongocxx::database& db,
                                     const mongocxx::client_session& session,
                                     bsoncxx::document::view cmdRequest,
                                     bsoncxx::stdx::string_view cursorResultsField,
                                     metrics::OperationContext& requestMetricsCtx,
                                     metrics::OperationContext& overallMetricsCtx);
};

}  // namespace genny::actor

#endif  // HEADER_4ACF7ADF_14B2_4444_81BA_A17636BAEDA1_INCLUDED
