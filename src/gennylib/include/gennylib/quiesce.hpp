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

#include <mongocxx/pool.hpp>
#include <string>

#include <gennylib/context.hpp>

namespace genny {

/**
 * Helper function to quiesce the system and reduce noise.
 * The appropriate actions will be taken whether the target
 * is a standalone, replica set, or sharded cluster.
 *
 * Note: This function is effectively in beta mode. We expect it to work, but
 * it hasn't been used extensively in production. Please let STM know of any
 * use so we can help monitor its effectiveness.
 */
bool quiesce(mongocxx::pool::entry& client,
             const std::string& dbName,
             const SleepContext& sleepContext);

}  // namespace genny
