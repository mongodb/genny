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

#ifndef HEADER_9F732905_0E78_4AE6_9473_5A7FEC25C430_INCLUDED
#define HEADER_9F732905_0E78_4AE6_9473_5A7FEC25C430_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Actor that run dbcheck on a replica set and waiting for it to finish.
 * Only one thread is allowed to run this actor at a time.
 *
 * ```yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: DbCheckActor
 *   Type: DbCheckActor
 *     Database: mydb
 *     Threads: 1
 *     Phases:
 *       - Repeat: 1
 *       Collection: mycoll
 *       ValidateMode: dataConsistency
 * ```
 *
 * Owner: mongodb/server-replication
 */
class DbCheckActor : public Actor {

public:
    explicit DbCheckActor(ActorContext& context);
    ~DbCheckActor() = default;

    void run() override;

    static std::string_view defaultName() {
        return "DbCheckActor";
    }

private:
    mongocxx::pool::entry _client;

    genny::metrics::Operation _dbcheckMetric;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_9F732905_0E78_4AE6_9473_5A7FEC25C430_INCLUDED
