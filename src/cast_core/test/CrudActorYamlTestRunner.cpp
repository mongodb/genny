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

#include <vector>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>
#include <testlib/yamlTest.hpp>

namespace genny::testing {

class CrudActorResult : public Result {};


struct CrudActorTestCase {
    using Result = CrudActorResult;

    explicit CrudActorTestCase() = default;

    explicit CrudActorTestCase(YAML::Node node)
        : description{node["Description"].as<std::string>()}, operations{node["Operations"]} {}

    Result run() const {
        Result out;
        return out;
    }

    std::string description;
    YAML::Node operations;
};

std::ostream& operator<<(std::ostream& out, const std::vector<CrudActorResult>& results) {
    out << std::endl;
    for (auto&& result : results) {
        out << "- Test Case" << std::endl;
        for (auto&& [expect, actual] : result.expectedVsActual()) {
            out << "    - " << actual << std::endl;
        }
    }
    return out;
}

}  // namespace genny::testing


namespace YAML {

template <>
struct convert<genny::testing::CrudActorTestCase> {
    static bool decode(YAML::Node node, genny::testing::CrudActorTestCase& out) {
        out = genny::testing::CrudActorTestCase(node);
        return true;
    }
};

}  // namespace YAML


namespace {
TEST_CASE("CrudActor YAML Tests",
          "[standalone][single_node_replset][three_node_replset][CrudActor]") {

    genny::testing::runTestCaseYaml<genny::testing::CrudActorTestCase>(
        "/src/cast_core/test/CrudActorYamlTests.yaml");
}
}  // namespace
