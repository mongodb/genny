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

#ifndef HEADER_A0EBCED8_5FE8_4EA0_9292_52531417B62E_INCLUDED
#define HEADER_A0EBCED8_5FE8_4EA0_9292_52531417B62E_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Bulk inserts data into a single collection using multiple threads. Similar to
 * the MonotonicLoader actor, the generated documents have a monotonically
 * increasing _id, starting from {_id: 0}.
 *
 * However, the MonotonicSingleLoader actor differs from the Loader and
 * MonotonicLoader actors in a few notable ways:
 *
 *  - The collection name is optional and defaults to "Collection0" if omitted.
 *
 *  - The MonotonicSingleLoader actor must only ever be active in one phase of the
 *    workload.
 *
 * ```yaml
 * SchemaVersion: 2018-07-01
 * Actors:
 * - Name: LoadInitialData
 *   Type: MonotonicSingleLoader
 *   Threads: 100
 *   Phases:
 *   - Repeat: 1
 *     Database: test
 *     Collection: mycoll
 *     BatchSize: 1000
 *     DocumentCount: 100000
 *     Document: {field: {^RandomInt: {min: 0, max: 100}}}
 * ```
 *
 * Owner: @mongodb/sharding
 */
class MonotonicSingleLoader : public Actor {
public:
    explicit MonotonicSingleLoader(ActorContext& context);
    ~MonotonicSingleLoader() = default;

    void run() override;

    static std::string_view defaultName() {
        return "MonotonicSingleLoader";
    }

private:
    mongocxx::pool::entry _client;

    metrics::Operation _totalBulkLoad;
    metrics::Operation _individualBulkLoad;

    // Used to assign documents a unique and montonically increasing _id.
    struct DocumentIdCounter : WorkloadContext::ShareableState<std::atomic_int64_t> {};
    DocumentIdCounter& _docIdCounter;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_A0EBCED8_5FE8_4EA0_9292_52531417B62E_INCLUDED
