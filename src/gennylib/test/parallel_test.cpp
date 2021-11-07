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
using Catch::Matchers::Contains;

TEST_CASE("Parallel runner runs op") {
    std::vector<int> integers = {1, 2, 3, 4, 5};
    AtomicVector<int> newIntegers;
    parallelRun(integers,
               [&](const auto& integer) {
                   newIntegers.push_back(integer + 5);
               });

    std::vector<int> expected = {6, 7, 8, 9, 10};

    std::lock_guard<AtomicVector<int>> newLock(newIntegers);
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

TEST_CASE("AtomicContainer throws if iterating without holding lock") {
    AtomicVector<int> integers;
    integers.push_back(6);
    integers.push_back(7);
    auto mustHoldLock = [&]() {
        for (auto&& integer : integers) {
            
        }
    };
    REQUIRE_THROWS_WITH(mustHoldLock(), Contains("Must forcibly hold container lock in order to iterate."));

    auto cannotLockTwice = [&]() {
        std::lock_guard<AtomicVector<int>> intLock1(integers);
        std::lock_guard<AtomicVector<int>> intLock2(integers);
    };

    REQUIRE_THROWS_WITH(cannotLockTwice(), Contains("Cannot forcibly hold AtomicContainer lock multiple times."));

    AtomicVector<int> newIntegers;
    auto safelyIterates = [&]() {
        std::lock_guard<AtomicVector<int>> intLock(integers);
        for (auto&& integer : integers) {
            newIntegers.push_back(integer + 1);
        }
    };
    safelyIterates();
    REQUIRE(newIntegers.size() == 2);
    REQUIRE(newIntegers[0] == 7);
    REQUIRE(newIntegers[1] == 8);
}

TEST_CASE("AtomicContainer locks when used") {
    using namespace std::chrono_literals;
    // One thread that starts soon, another thread that waits then is held up.
    std::vector<std::chrono::milliseconds> millis = {100ms, 400ms};
    AtomicVector<std::chrono::milliseconds> outputs;
    AtomicVector<std::chrono::milliseconds> outputs2;
    auto test = [&]() {
        parallelRun(millis,
                   [&](const auto& millis) {
                       std::this_thread::sleep_for(millis);
                       // Second thread should block here.
                       outputs.push_back(millis);
                       std::lock_guard<AtomicVector<std::chrono::milliseconds>> lk(outputs);
                       outputs.push_back(millis);
                       std::this_thread::sleep_for(600ms);
                       for (const auto& i : outputs) {
                           outputs2.push_back(millis);
                       }
                   });
    };

    test();

    std::vector<std::chrono::milliseconds> expected = {100ms, 100ms, 400ms, 400ms};
    std::vector<std::chrono::milliseconds> expected2 = {100ms, 100ms, 400ms, 400ms, 400ms, 400ms};
    REQUIRE(expected.size() == outputs.size());
    REQUIRE(expected2.size() == outputs2.size());

    std::lock_guard<AtomicVector<std::chrono::milliseconds>> lk(outputs);
    for (int i = 0; i < expected.size(); i++) {
        REQUIRE(outputs[i] == expected[i]);
    }
    std::lock_guard<AtomicVector<std::chrono::milliseconds>> lk2(outputs2);
    for (int i = 0; i < expected2.size(); i++) {
        REQUIRE(outputs2[i] == expected2[i]);
    }
}
