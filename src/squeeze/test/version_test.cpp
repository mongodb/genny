#include "test.h"
// TODO: how to make this <squeeze/version.h>
#include <version.h>

SCENARIO("We have the right version") {
    REQUIRE( squeeze::lib::get_version() == "0.0.1" );
}

