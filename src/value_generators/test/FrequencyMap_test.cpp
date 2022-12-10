// Copyright 2023-present MongoDB Inc.
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


#include <algorithm>
#include <iostream>

#include <catch2/catch_all.hpp>

#include <value_generators/FrequencyMap.hpp>

namespace genny {
namespace {

TEST_CASE("genny frequencyMap") {

    SECTION("Basic") {
        FrequencyMap map;

        // Load Map
        map.push_back("a", 1);

        REQUIRE(map.size() == 1);
        REQUIRE(map.total_count() == 1);

        map.push_back("b", 3);

        REQUIRE(map.size() == 2);
        REQUIRE(map.total_count() == 4);

        map.push_back("c", 5);

        REQUIRE(map.size() == 3);
        REQUIRE(map.total_count() == 9);

        // Read from map
        auto b1 = map.take(1);
        REQUIRE(b1 == "b");

        REQUIRE(map.size() == 3);
        REQUIRE(map.total_count() == 8);

        auto b2 = map.take(1);
        REQUIRE(b2 == "b");

        REQUIRE(map.size() == 3);
        REQUIRE(map.total_count() == 7);
        auto b3 = map.take(1);
        REQUIRE(b3 == "b");

        REQUIRE(map.size() == 2);
        REQUIRE(map.total_count() == 6);
    }
}

}  // namespace
}  // namespace genny
