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

#include <vector>
#include <string>
#include <util>

namespace genny::testing {

template<typename TC>
class ResultT {
public:
    explicit ResultT(const TC& testCase) : _testCase{testCase} {}

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

    const TC& testCase() const {
        return _testCase;
    }

private:
    const TC& _testCase;
    std::vector<std::pair<std::string, std::string>> _expectedVsActual;
    bool _expectedExceptionButNotThrown = false;
};


template<typename TC>
class YamlTests {
public:
    using Result = typename TC::Result;

    explicit YamlTests() = default;
    YamlTests(const YamlTests&) = default;

    explicit YamlTests(const YAML::Node& node) {
        for (auto&& n : node["Cases"]) {
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
    std::vector<Result> _cases;
};




}  // namespae genny::testing


#endif  // HEADER_8A80B79E_1A24_4745_BB59_B1FCD1439C6D_INCLUDED
