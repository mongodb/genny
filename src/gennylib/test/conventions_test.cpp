#include "test.h"

#include <chrono>

#include <yaml-cpp/yaml.h>

#include <gennylib/conventions.hpp>

using namespace std::chrono;

TEST_CASE("Millisecond conversions") {
    SECTION("Can convert to milliseconds") {
        REQUIRE( YAML::Load("D: 300")["D"].as<milliseconds>().count() == 300 );
        REQUIRE( YAML::Load("-1").as<milliseconds>().count() == -1 );
        REQUIRE( YAML::Load("0").as<milliseconds>().count() == 0 );
    }

    SECTION("Barfs on unknown types") {
        REQUIRE_THROWS( YAML::Load("foo").as<milliseconds>() );
        REQUIRE_THROWS( YAML::Load("[1,2,3]").as<milliseconds>() );
        REQUIRE_THROWS( YAML::Load("[]").as<milliseconds>() );
        REQUIRE_THROWS( YAML::Load("{}").as<milliseconds>() );
        REQUIRE_THROWS( YAML::Load("foo: 1").as<milliseconds>() );
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Duration"] = milliseconds{30};
        REQUIRE(n["Duration"].as<milliseconds>().count() == 30);
    }

    // this section goes away once we implement desired support for richer parsing of strings to duration
    SECTION("Future convention support") {
        REQUIRE_THROWS( YAML::Load("1 milliseconds").as<milliseconds>() );
    }
}

