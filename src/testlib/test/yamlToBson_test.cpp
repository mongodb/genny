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

#include <string>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>

#include <testlib/helpers.hpp>
#include <testlib/yamlToBson.hpp>

namespace genny::testing {

using Catch::Matchers::Matches;

namespace {
bsoncxx::document::value bson(const std::string& json) {
    return bsoncxx::from_json(json);
}
std::string json(const bsoncxx::document::view_or_value& doc) {
    return bsoncxx::to_json(doc, bsoncxx::ExtendedJsonMode::k_canonical);
}
YAML::Node yaml(const std::string& str) {
    return YAML::Load(str);
}
bsoncxx::document::value fromYaml(const std::string& yamlStr) {
    return toDocumentBson(yaml(yamlStr));
}


void test(const std::string& yaml, const bsoncxx::document::view_or_value& expect) {
    try {
        auto actual = fromYaml(yaml);
        INFO(bsoncxx::to_json(expect) << " == " << bsoncxx::to_json(actual));
        REQUIRE(expect.view() == actual.view());
    } catch (const std::exception& x) {
        WARN(yaml << " => " << json << " ==> " << x.what());
        throw;
    }
}

void test(const std::string& yaml, const std::string& json) {
    return test(yaml, bson(json));
}


namespace BasicBson = bsoncxx::builder::basic;
namespace BsonTypes = bsoncxx::types;

TEST_CASE("YAML To BSON Simple") {
    // use the smallest type that will fit
    test("foo: 0", BasicBson::make_document(BasicBson::kvp("foo", BsonTypes::b_int32{0})));

    // max valeu for int32 ↓
    test("foo: 2147483647",
         BasicBson::make_document(BasicBson::kvp("foo", BsonTypes::b_int32{2147483647})));
    //            ↑ + 1
    test("foo: 2147483648",
         BasicBson::make_document(BasicBson::kvp("foo", BsonTypes::b_int64{2147483648})));

    // max value for int64 ↓
    test("foo: 9223372036854775807",
         BasicBson::make_document(BasicBson::kvp("foo", BsonTypes::b_int64{9223372036854775807})));

    test("foo: bar", R"({"foo":"bar"})");
    test("foo: '0'", R"({"foo":"0"})");
    test("foo: 1", R"({"foo":1})");
    test("foo: 1.0", R"({"foo":1.0})");
    test("foo: null", R"({"foo":null})");
    test("foo: true", R"({"foo":true})");
    test("foo: false", R"({"foo":false})");
    test("foo: yes", R"({"foo":true})");
    test("foo: off", R"({"foo":false})");


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

TEST_CASE("YAML with Anchors") {
    test(R"(
included: &inc
  Frodo: Baggins
  Gimli: Son of Glóin
foo: *inc
)",
         R"(
{ "included" : { "Frodo" : "Baggins", "Gimli" : "Son of Glóin" },
  "foo" : { "Frodo" : "Baggins", "Gimli" : "Son of Glóin" } }
)");
}

}  // namespace
}  // namespace genny::testing
