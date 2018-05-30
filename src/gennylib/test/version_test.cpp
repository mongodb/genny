#include "test.h"

#include <gennylib/version.hpp>

SCENARIO("We have the right version") {
    REQUIRE( genny::lib::get_version() == "0.0.1" );
}

