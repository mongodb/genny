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


TEST_CASE("Parallel runner runs op") {
    std::vector<int> integers = {1, 2, 3, 4, 5};
    std::vector<int> newIntegers;
    std::mutex integerLock;
    parallelRun(integers,
               [&](const auto& integer) {
                   std::lock_guard<std::mutex> lk(integerLock);
                   newIntegers.push_back(integer + 5);
               });

    std::vector<int> expected = {6, 7, 8, 9, 10};

    REQUIRE(expected.size() == newIntegers.size());
    for (int i = 0; i < expected.size(); i++) {
        REQUIRE(std::find(newIntegers.begin(), newIntegers.end(),
                    expected[i]) != newIntegers.end());
    }
}
