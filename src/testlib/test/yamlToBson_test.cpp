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

#include <ios>
#include <string>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <iostream>
#include <testlib/helpers.hpp>
#include <testlib/yamlToBson.hpp>

namespace genny::testing {

using Catch::Matchers::Matches;

namespace {
bsoncxx::document::view_or_value bson(const std::string& json) {
    return bsoncxx::from_json(json);
}
std::string json(const bsoncxx::document::view_or_value& doc) {
    return bsoncxx::to_json(doc, bsoncxx::ExtendedJsonMode::k_canonical);
}
YAML::Node yaml(const std::string& str) {
    return YAML::Load(str);
}
bsoncxx::document::view_or_value fromYaml(const std::string& yamlStr) {
    return toDocumentBson(yaml(yamlStr));
}

void test(const std::string& yaml, const std::string& json) {
    try {
        auto actual = fromYaml(yaml);
        auto expect = bson(json);
        INFO(bsoncxx::to_json(expect) << bsoncxx::to_json(actual));
        REQUIRE(expect == actual);
    } catch (const std::exception& x) {
        WARN(yaml << " => " << json << " ==> " << x.what());
        throw;
    }
}
}  // namespace


TEST_CASE("YAML To BSON Simple") {
    test("foo: bar", R"({"foo":"bar"})");
    test("foo: 1", R"({"foo":1})");
    test("foo: null", R"({"foo":null})");
    test("foo: true", R"({"foo":true})");

    test("foo: {}", R"({"foo":{}})");
    test("foo: []", R"({"foo":[]})");
    test("foo: [[]]", R"({"foo":[[]]})");
    test("foo: [{}]", R"({"foo":[{}]})");
    test("foo: [1,{}]", R"({"foo":[1,{}]})");

    test("foo: [10.1]", R"({"foo":[10.1]})");
    test("foo: [10.1,]", R"({"foo":[10.1]})");

}

TEST_CASE("YAML To BSON Nested") {
    test(R"(
foo:
  bar:
  - some
  - mixed: [list]
)",
         R"(
{ "foo" : { "bar" : [ "some", { "mixed" : [ "list" ] } ] } }
)");
}

}  // namespace genny::testing
