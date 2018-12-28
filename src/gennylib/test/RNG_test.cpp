#include "test.h"

#include <algorithm>
#include <iostream>

#include <gennylib/RNG.hpp>

namespace genny {

TEST_CASE("genny RNG") {

    SECTION("Can be used in std::shuffle") {
        std::vector<int> input = {1, 2, 3};
        std::vector<int> output = {3, 1, 2};

        DefaultRNG rng;
        rng.seed(12345);

        std::shuffle(input.begin(), input.end(), rng);

        REQUIRE(input == output);
    }
}
}  // namespace genny
