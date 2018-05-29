#include "test.h"

#include <genny/version.h>

SCENARIO("We have the right version") {
    REQUIRE( genny::lib::get_version() == "0.0.1" );
}

