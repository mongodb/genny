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

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <boost/log/trivial.hpp>

#include <gennylib/parallel.hpp>

#include <testlib/helpers.hpp>

using namespace genny;
using Catch::Matchers::Matches;

TEST_CASE("Parallel runner runs op") {
    std::vector<int> integers = {1, 2, 3, 4, 5};
    AtomicVector<int> newIntegers;
    parallelRun(integers,
               [&](const auto& integer) {
                   newIntegers.push_back(integer + 5);
               });

    std::vector<int> expected = {6, 7, 8, 9, 10};

    REQUIRE(expected.size() == newIntegers.size());
    for (int i = 0; i < expected.size(); i++) {
        // The order of elements in newIntegers is nondeterministic.
        REQUIRE(std::find(newIntegers.begin(), newIntegers.end(),
                    expected[i]) != newIntegers.end());
    }
}

TEST_CASE("Parallel runner reraises exceptions") {
    std::vector<int> integers = {1, 2, 3, 4, 5};
    auto test = [&]() {
        parallelRun(integers,
                   [&](const auto& integer) {
                       throw std::logic_error("This should be reraised.");
                   });
    };
    REQUIRE_THROWS_WITH(test(), Matches("This should be reraised."));
}
