#include "test.h"

#include <algorithm>
#include <iostream>

#include <gennylib/PseudoRandom.hpp>

namespace genny {

TEST_CASE("genny PseudoRandom") {

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
}  // namespace genny
