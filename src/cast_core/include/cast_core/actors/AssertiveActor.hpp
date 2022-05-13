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

#ifndef HEADER_9F01CA6C_0D34_4966_A1E0_1BAFCAB49528_INCLUDED
#define HEADER_9F01CA6C_0D34_4966_A1E0_1BAFCAB49528_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * This actor asserts that the results of two aggregations (or any two valid database commands)
 * return equivalent results. This is primarily useful for validating the TPC-H workload queries.
 *
 * ```yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: AssertiveActor
 *   Type: AssertiveActor
 *   Phases:
 *   - Repeat: 1
 *     Database: test
 *     Message: coll1 documents with foo=1 are identical to coll2 documents
 *     Expected:
 *       aggregate: coll1
 *       pipeline: [{$match: {foo: 1}}, {$sort: {_id: 1}}]
 *       cursor: {batchSize: 101}
 *     Actual:
 *       aggregate: coll2
 *       pipeline: [{$sort: {_id: 1}}]
 *       cursor: {batchSize: 101}
 * ```
 *
 * Owner: "@mongodb/query"
 */

class AssertFailed : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

class AssertiveActor : public Actor {
public:
    explicit AssertiveActor(ActorContext& context);
    ~AssertiveActor() = default;

    void run() override;

    static std::string_view defaultName() {
        return "AssertiveActor";
    }

    struct PhaseConfig;
private:
    mongocxx::pool::entry _client;
    genny::metrics::Operation _assert;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_9F01CA6C_0D34_4966_A1E0_1BAFCAB49528_INCLUDED
