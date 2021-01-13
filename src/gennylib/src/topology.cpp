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

#include <string>
#include <optional>
#include <mongocxx/database.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/topology.hpp>


namespace genny {

void Topology::update(mongocxx::pool::entry& client) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;


    auto admin = client->database("admin");

    bool isMongos = false;

    auto res = admin.run_command(make_document(kvp("isMaster", 1)));
    auto msg = res.view()["msg"];
    if (msg && msg.type() == bsoncxx::type::k_utf8) {
        isMongos = msg.get_utf8().value == "isdbgrid";
    }

    if (isMongos) {
        BOOST_LOG_TRIVIAL(error) << "Connected to a mongos";
    } else {
        BOOST_LOG_TRIVIAL(error) << "Connected to a mongod";
    }
}

}  // namespace genny
