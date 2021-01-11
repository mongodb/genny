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

#include <string_view>

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

namespace genny {

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

void doFSyncReplicaSet(mongocxx::pool::entry& client) {
    auto admin = client->database("admin");
    auto status = admin.run_command(make_document(kvp("replSetGetStatus", 1))).view();
    if (!status["ok"].get_bool()) {
        // TODO: Throw exception
    }
    if (status.find("members") != status.cend()) {
        bsoncxx::array::view members = status["members"].get_array();
        for (auto member : members) {
            auto name = member["name"].get_utf8().value;
            mongocxx::uri uri(name);
            mongocxx::client memberClient(uri);
            auto memberAdmin = memberClient.database("admin");
            memberAdmin.run_command(make_document(kvp("fsync", 1)));
        }
    }
}

/*
 * Helper function to quiesce the system and reduce noise.
 * The appropriate actions will be taken whether the target
 * is a standalone, replica set, or sharded cluster.
 */
template <class entry = mongocxx::pool::entry>
bool quiesceImpl(entry& client) {
    return true;
}

}

#endif  // HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED

