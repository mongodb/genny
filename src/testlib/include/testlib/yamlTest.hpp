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

#include <yaml-cpp/yaml.h>

#include <testlib/findRepoRoot.hpp>

namespace genny::testing::v1 {

class Result {
public:
    bool pass() const {
        return _expectedVsActual.empty() && !_expectedExceptionButNotThrown;
    }

    template <typename E, typename A>
    void expectEqual(E expect, A actual) {
        if (expect != actual) {
            _expectedVsActual.emplace_back(toString(expect), toString(actual));
        }
    }

    void expectedExceptionButNotThrown() {
        _expectedExceptionButNotThrown = true;
    }

    const std::vector<std::pair<std::string, std::string>>& expectedVsActual() const {
        return _expectedVsActual;
    }

private:
    std::vector<std::pair<std::string, std::string>> _expectedVsActual;
    bool _expectedExceptionButNotThrown = false;
};


template <typename TC>
class YamlTests {
public:
    using Result = typename TC::Result;

    explicit YamlTests() = default;
    YamlTests(const YamlTests&) = default;

    explicit YamlTests(const YAML::Node& node) {
        for (auto&& n : node["Tests"]) {
            _cases.push_back(std::move(n.as<TC>()));
        }
    }

    std::vector<Result> run() const {
        std::vector<Result> out{};
        for (auto& tcase : _cases) {
            auto result = tcase.run();
            if (!result.pass()) {
                out.push_back(std::move(result));
            }
        }
        return out;
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

using Result = v1::Result;

template <typename TC>
void runTestCaseYaml(const std::string& repoRelativePathToYaml) {
    try {
        const auto file = genny::findRepoRoot() + repoRelativePathToYaml;
        const auto yaml = ::YAML::LoadFile(file);
        auto tests = yaml.as<genny::testing::v1::YamlTests<TC>>();
        auto results = tests.run();
        if (!results.empty()) {
            std::stringstream msg;
            msg << results;
            WARN(msg.str());
        }
        REQUIRE(results.empty());
    } catch (const std::exception& ex) {
        WARN(ex.what());
        throw;
    }
}

}  // namespace genny::testing


#endif  // HEADER_8A80B79E_1A24_4745_BB59_B1FCD1439C6D_INCLUDED
