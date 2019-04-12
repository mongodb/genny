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

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>

namespace {

genny::DefaultRandom rng;

std::string toString(const std::string& str) {
    return str;
}

std::string toString(const bsoncxx::document::view_or_value& t) {
    return bsoncxx::to_json(t, bsoncxx::ExtendedJsonMode::k_canonical);
}

std::string toString(const YAML::Node& node) {
    YAML::Emitter out;
    out << node;
    return std::string{out.c_str()};
}

}  // namespace



namespace {
TEST_CASE("CrudActor YAML Tests", "[standalone][single_node_replset][three_node_replset][CrudActor]") {


}
}
