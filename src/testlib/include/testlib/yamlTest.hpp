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

#ifndef HEADER_8A80B79E_1A24_4745_BB59_B1FCD1439C6D_INCLUDED
#define HEADER_8A80B79E_1A24_4745_BB59_B1FCD1439C6D_INCLUDED

#include <string>
#include <vector>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/findRepoRoot.hpp>
#include <testlib/helpers.hpp>

namespace genny::testing::v1 {

template <typename TC>
class YamlTests {
public:
    explicit YamlTests() = default;
    YamlTests(const YamlTests&) = default;

    explicit YamlTests(const YAML::Node& node) {
        for (auto&& n : node["Tests"]) {
            _cases.push_back(std::move(n.as<TC>()));
        }
    }

    void run() const {
        for (auto& tcase : _cases) {
            tcase.run();
        }
    }

private:
    std::vector<TC> _cases;
};

}  // namespace genny::testing::v1

namespace YAML {

template <typename TC>
struct convert<genny::testing::v1::YamlTests<TC>> {
    static Node encode(const genny::testing::v1::YamlTests<TC>& rhs) {
        return {};
    }
    static bool decode(const Node& node, genny::testing::v1::YamlTests<TC>& rhs) {
        rhs = genny::testing::v1::YamlTests<TC>(node);
        return true;
    }
};

}  // namespace YAML

namespace genny::testing {

template <typename TC>
void runTestCaseYaml(const std::string& repoRelativePathToYaml) {
    try {
        const auto file = genny::findRepoRoot() + repoRelativePathToYaml;
        const auto yaml = ::YAML::LoadFile(file);
        auto tests = yaml.as<genny::testing::v1::YamlTests<TC>>();
        tests.run();
    } catch (const std::exception& ex) {
        WARN(boost::diagnostic_information(ex));
        throw;
    }
}

}  // namespace genny::testing


#endif  // HEADER_8A80B79E_1A24_4745_BB59_B1FCD1439C6D_INCLUDED
