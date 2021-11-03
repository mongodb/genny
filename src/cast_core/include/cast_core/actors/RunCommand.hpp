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

#ifndef HEADER_32412A69_F128_4BC8_8335_520EE35F5381
#define HEADER_32412A69_F128_4BC8_8335_520EE35F5381

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * RunCommand is an actor that performs database and admin commands on a database. The
 * actor records the latency of each command run. If no database value is provided for
 * an actor, then the operations will run on the 'admin' database by default.
 *
 *
 * Example:
 *
 * ```yaml
 * Actors:
 * - Name: MultipleOperations
 *   Type: RunCommand
 *   Database: test
 *   Operations:
 *   - MetricsName: ServerStatus
 *     OperationName: RunCommand
 *     OperationCommand:
 *       serverStatus: 1
 *   - OperationName: RunCommand
 *     OperationCommand:
 *       find: scores
 *       filter: { rating: { $gte: 50 } }
 * - Name: SingleAdminOperation
 *   Type: AdminCommand
 *   Phases:
 *   - Repeat: 5
 *     MetricsName: CurrentOp
 *     Operation:
 *       OperationName: RunCommand
 *       OperationCommand:
 *         currentOp: 1
 * ```
 *
 * Owner: STM
 */
class RunCommand : public Actor {
public:
    explicit RunCommand(ActorContext& context, ActorId id);
    ~RunCommand() override = default;

    static std::string_view defaultName() {
        return "RunCommand";
    }

    void run() override;

private:
    mongocxx::pool::entry _client;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};


}  // namespace genny::actor

#endif  // HEADER_32412A69_F128_4BC8_8335_520EE35F5381
