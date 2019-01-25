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


#include <algorithm>
#include <iostream>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <value_generators/DefaultRandom.hpp>

namespace genny {

namespace {
TEST_CASE("genny DefaultRandom") {

    SECTION("Can be used in std::shuffle") {
        auto output = std::vector<int>(100);
        std::iota(std::begin(output), std::end(output), 1);

        DefaultRandom rng;
        rng.seed(12345);

        const auto input = output;
        std::shuffle(output.begin(), output.end(), rng);

        REQUIRE(input != output);
    }
}
}  // namespace

}  // namespace genny
