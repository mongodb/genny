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

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

void Topology::getDataMemberConnectionStrings(mongocxx::pool::entry& client) {
    auto admin = client->database("admin");
    auto res = admin.run_command(make_document(kvp("isMaster", 1)));
    if (!res.view()["setName"]) {
        std::unique_ptr<MongodDescription> desc = std::make_unique<MongodDescription>();
        desc->mongodUri = client->uri().to_string();
        this->_topology.reset(desc.release());
        return;
    }

    auto primary = res.view()["primary"];
    //bsoncxx::array::view passives = res.view()["passives"].get_array();

    std::unique_ptr<ReplSetDescription> desc = std::make_unique<ReplSetDescription>();
    desc->primaryUri = std::string(primary.get_utf8().value);

    auto hosts = res.view()["hosts"];
    if (hosts && hosts.type() == bsoncxx::type::k_array) {
        bsoncxx::array::view hosts_view = hosts.get_array();
        for (auto member : hosts_view) {
            MongodDescription memberDesc;
            memberDesc.mongodUri = std::string(member.get_utf8().value);
            desc->nodes.push_back(memberDesc);
        }
    }

    auto passives = res.view()["passives"];
    if (passives && passives.type() == bsoncxx::type::k_array) {
        bsoncxx::array::view passives_view = passives.get_array();
        for (auto member : passives_view) {
            MongodDescription memberDesc;
            memberDesc.mongodUri = std::string(member.get_utf8().value);
            desc->nodes.push_back(memberDesc);
        }
    }

    this->_topology.reset(desc.release());
}

void Topology::findConnectedNodesViaMongos(mongocxx::pool::entry& client) {

}

void Topology::update(mongocxx::pool::entry& client) {
    auto admin = client->database("admin");

    bool isMongos = false;
    auto res = admin.run_command(make_document(kvp("isMaster", 1)));
    auto msg = res.view()["msg"];
    if (msg && msg.type() == bsoncxx::type::k_utf8) {
        isMongos = msg.get_utf8().value == "isdbgrid";
    }

    if (isMongos) {
        findConnectedNodesViaMongos(client);
    } else {
        getDataMemberConnectionStrings(client);
    }

    ToJsonVisitor visitor;
    accept(visitor);
    BOOST_LOG_TRIVIAL(error) << "output: " << visitor.str();
}

}  // namespace genny
