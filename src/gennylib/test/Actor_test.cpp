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

#include <gennylib/Actor.hpp>

#include <testlib/helpers.hpp>

#include <boost/log/trivial.hpp>

using namespace genny;

TEST_CASE("TaskQueue stores and resolves tasks") {

    using TestResultT = std::unique_ptr<std::string>;
   
    TaskQueue testQueue;

    std::string sideEffect1 = "no side effect 1";
    TaskResult<TestResultT> res1 = testQueue.addTask<TestResultT>([&]() {
            sideEffect1 = "caused side effect 1!";
            return std::make_unique<std::string>("true");
            });

    std::string sideEffect2 = "no side effect 2";
    TaskResult<TestResultT> res2 = testQueue.addTask<TestResultT>([&]() {
            sideEffect2 = "caused side effect 2!";
            return std::make_unique<std::string>("true");
            });

    // If the type is not dereferenceable (* and ->), that's fine as
    // long as we don't call those.
    std::string sideEffect3 = "no side effect 3";
    TaskResult<std::string> res3 = testQueue.addTask<std::string>([&]() {
            sideEffect3 = "caused side effect 3!";
            return std::string("true");
            });



    // Have not resolved anything yet.
    REQUIRE(sideEffect1 == "no side effect 1");
    REQUIRE(sideEffect2 == "no side effect 2");
    REQUIRE(sideEffect3 == "no side effect 3");
    REQUIRE(!res1.isResolved());
    REQUIRE(!res2.isResolved());
    REQUIRE(!res3.isResolved());

    res2.resolve();

    // We resolved 2 early, causing the future to resolve / execute.
    REQUIRE(sideEffect1 == "no side effect 1");
    REQUIRE(sideEffect2 == "caused side effect 2!");
    REQUIRE(sideEffect3 == "no side effect 3");
    REQUIRE(!res1.isResolved());
    REQUIRE(res2.isResolved());
    REQUIRE(!res3.isResolved());

    REQUIRE(res1->size() == 4);

    // We accessed 1, causing the future to resolve / execute.
    REQUIRE(sideEffect1 == "caused side effect 1!");
    REQUIRE(sideEffect2 == "caused side effect 2!");
    REQUIRE(sideEffect3 == "no side effect 3");
    REQUIRE(res1.isResolved());
    REQUIRE(res2.isResolved());
    REQUIRE(!res3.isResolved());

    sideEffect1 = "resolved tasks aren't rerun";
    testQueue.runAllTasks();

    // The futures are ready; the container is resolved.
    REQUIRE(sideEffect1 == "resolved tasks aren't rerun");
    REQUIRE(sideEffect2 == "caused side effect 2!");
    REQUIRE(sideEffect3 == "caused side effect 3!");
    REQUIRE(res1.isResolved());
    REQUIRE(res2.isResolved());
    REQUIRE(res3.isResolved());
}
