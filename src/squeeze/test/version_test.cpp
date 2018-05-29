#include "test.h"

#include <squeeze/version.h>

SCENARIO("We have the right version") {
    REQUIRE( squeeze::lib::get_version() == "0.0.1" );
}

