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

#ifndef HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED
#define HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED

#include <string_view>

#include <mongocxx/database.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/**
 * CrudActor is an actor that performs CRUD operations on a collection and has the ability to start
 * and commit transactions. This actor aims to support the operations in the mongocxx-driver
 * collections API. The actor supports the usage of a list of operations for a single phase.
 *
 * Example:
 *
 * ```yaml
 * Actors:
 * - Name: BulkWriteInTransaction
 *   Type: CrudActor
 *   Database:
 *   Type: CrudActor
 *   Database: testdb
 *   Phases:
 *   - Repeat: 1
 *     Collection: test
 *     Operations:
 *     - OperationName: startTransaction
 *       OperationCommand:
 *         Options:
 *           WriteConcern:
 *             Level: majority
 *             Journal: true
 *           ReadConcern:
 *             Level: snapshot
 *           ReadPreference:
 *             ReadMode: primaryPreferred
 *             MaxStalenessSeconds: 1000
 *     - OperationName: bulkWrite
 *       OperationCommand:
 *         WriteOperations:
 *         - WriteCommand: insertOne
 *           Document: { a: 1 }
 *         - WriteCommand: updateOne
 *           Filter: { a: 1 }
 *           Update: { $set: { a: 5 } }
 *         Options:
 *           Ordered: true
 *           WriteConcern:
 *             Level: majority
 *         OnSession: true
 *     - OperationName: commitTransaction
 *   - Repeat: 1
 *     Collection: test
 *     Operation:
 *       OperationName: drop
 * ```
 *
 * Owner: STM
 */
class CrudActor : public Actor {

public:
    class Operation;

public:
    explicit CrudActor(ActorContext& context);
    ~CrudActor() = default;

    static std::string_view defaultName() {
        return "CrudActor";
    }

    void run() override;

private:
    mongocxx::pool::entry _client;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_338F3734_B052_443C_A358_60215BA9D5A0_INCLUDED
