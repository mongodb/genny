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

#ifndef HEADER_MAT_VIEW_ACTOR_INCLUDED
#define HEADER_MAT_VIEW_ACTOR_INCLUDED

#include <string_view>

#include <mongocxx/database.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/**
 * Example:
 * ```yaml

 * Actors:
 * - Name: UpdateDocumentsInTransactionActor
 *   Type: MatViewActor
 *   Database: &db test
 *   Threads: 32
 *   Phases:
 *   - MetricsName: MaintainView
 *     Repeat: *numInsertBatchesPerClinet
 *     Database: *db
 *     Collection: Collection0
 *     Operations:
 *     - OperationName: matView
 *         OperationCommand:
 *           Debug: false
 *           Database: *db
 *           ThrowOnFailure: false
 *           RecordFailure: true
 *           InsertDocument:
 *             k: {^Inc: {start: 0}}
 *           TransactionOptions:
 *             MaxCommitTime: 500 milliseconds
 *             WriteConcern:
 *               Level: majority
 *               Journal: true
 *             ReadConcern:
 *               Level: snapshot
 *             ReadPreference:
 *               ReadMode: primaryPreferred
 *               MaxStaleness: 1000 seconds
 * ```
 *
 * Owner: Query
 */
class MatViewActor : public genny::Actor {

public:
    /**
     * Construct a MatViewActor.
     * @param context represents the `Actor` block.
     */
    explicit MatViewActor(ActorContext& context);

    /** Destruct */
    ~MatViewActor() override = default;

    /** @return name to use for metrics etc. */
    static std::string_view defaultName() {
        return "MatViewActor";
    }

    void run() override;

private:
    mongocxx::pool::entry _client;

    /**
     * constructed from each `Phase:` block in the `Actor:` block
     * @private
     */
    struct PhaseConfig;
    struct CollectionName;
    /** loops over each Phase and handles Duration/Repeat/GlobalRate */
    PhaseLoop<PhaseConfig> _loop;
    genny::DefaultRandom& _rng;
};

}  // namespace genny::actor

#endif  // HEADER_MAT_VIEW_ACTOR_INCLUDED
