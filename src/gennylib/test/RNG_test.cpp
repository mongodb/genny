#include "test.h"

#include <algorithm>
#include <iostream>

#include <gennylib/PseudoRandom.hpp>

namespace genny {

TEST_CASE("genny PseudoRandom") {

    SECTION("Can be used in std::shuffle") {
        std::vector<int> input = {1, 2, 3};

        DefaultRandom rng;
        rng.seed(12345);

        std::shuffle(input.begin(), input.end(), rng);

        REQUIRE(input.size() == 3);
    }
}
}  // namespace genny
