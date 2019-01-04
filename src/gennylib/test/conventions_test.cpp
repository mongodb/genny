#include "test.h"

#include <chrono>

#include <yaml-cpp/yaml.h>

#include <gennylib/conventions.hpp>
#include <gennylib/time.hpp>

using namespace genny;

TEST_CASE("Millisecond conversions") {
    SECTION("Can convert to milliseconds") {
        REQUIRE(time::millis(YAML::Load("-1").as<time::Duration>()) == -1);
        REQUIRE(time::millis(YAML::Load("0").as<time::Duration>()) == 0);
        REQUIRE(time::millis(YAML::Load("{Unit: ms, Ticks: 300}").as<time::Duration>()) == 300);
        REQUIRE(time::millis(YAML::Load("{Unit: us, Ticks: 3000}").as<time::Duration>()) == 3);
        REQUIRE(time::millis(YAML::Load("{Unit: s, Ticks: 3}").as<time::Duration>()) == 3000);
        REQUIRE(time::millis(YAML::Load("D: 300")["D"].as<time::Duration>()) == 300);
    }

    SECTION("Barfs on unknown types") {
        REQUIRE_THROWS(YAML::Load("foo").as<time::Duration>());
        REQUIRE_THROWS(YAML::Load("[1,2,3]").as<time::Duration>());
        REQUIRE_THROWS(YAML::Load("[]").as<time::Duration>());
        REQUIRE_THROWS(YAML::Load("{}").as<time::Duration>());
        REQUIRE_THROWS(YAML::Load("foo: 1").as<time::Duration>());
    }

    SECTION("Can encode") {
        YAML::Node node;

        auto originalDuration = time::Duration{std::chrono::milliseconds{30}};
        node["Duration"] = originalDuration;
        auto decodedDuration = node["Duration"].as<time::Duration>();
        REQUIRE(time::millis(originalDuration) == time::millis(decodedDuration));
    }

    // this section goes away once we implement desired support for richer parsing of strings to
    // duration
    SECTION("Future convention support") {
        REQUIRE_THROWS(YAML::Load("1 milliseconds").as<time::Duration>());
    }
}
