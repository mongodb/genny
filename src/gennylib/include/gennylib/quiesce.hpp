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

#ifndef HEADER_5936639A_7D22_4629_8FE1_4A08443DDB0F_INCLUDED
#define HEADER_5936639A_7D22_4629_8FE1_4A08443DDB0F_INCLUDED

#include <mongocxx/database.hpp>

namespace genny {

/*
 * Helper function to quiesce the system and reduce noise.
 * The appropriate actions will be taken whether the target
 * is a standalone, replica set, or sharded cluster.
 */
template <class database = mongocxx::database>
bool quiesceImpl(database& targetDb) {
    return true;
}

}

#endif  // HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED

