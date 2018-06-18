#include "test.h"

#include <gennylib/version.hpp>

TEST_CASE("We have the right version") {
    REQUIRE(genny::getVersion() == "0.0.1");
}
