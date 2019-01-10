#include "test.h"

#include <chrono>
#include <cmath>

#include <yaml-cpp/yaml.h>

#include <gennylib/conventions.hpp>

namespace genny {
namespace {
using namespace std::chrono;

TEST_CASE("genny::Time conversions") {
    SECTION("Can convert to genny::Time") {
        REQUIRE(YAML::Load("D: 3 seconds")["D"].as<Time>().count() == 3 * std::pow(10, 9));
        REQUIRE(YAML::Load("-1 nanosecond").as<Time>().count() == -1);
        REQUIRE(YAML::Load("0 second").as<Time>().count() == 0);
        REQUIRE(YAML::Load("20 millisecond").as<Time>().count() == 20 * std::pow(10, 6));
        REQUIRE(YAML::Load("33 microsecond").as<Time>().count() == 33 * std::pow(10, 3));
        REQUIRE(YAML::Load("2e3 microseconds").as<Time>().count() == 2000 * std::pow(10, 3));
        REQUIRE(YAML::Load("10.3e2 nanoseconds").as<Time>().count() == 1030);
        REQUIRE(YAML::Load("3 hour").as<Time>().count() == 3 * 3600 * std::pow(10, 9));
        REQUIRE(YAML::Load("2 minutes").as<Time>().count() == 2 * 60 * std::pow(10, 9));
    }

    SECTION("Overlooks small typos") {
        REQUIRE(YAML::Load("D: 3 secondsasdfadsf     ")["D"].as<Time>().count() ==
                3 * std::pow(10, 9));
    }

    SECTION("Barfs on unknown types") {
        REQUIRE_THROWS(YAML::Load("foo").as<Time>());
        REQUIRE_THROWS(YAML::Load("[1,2,3]").as<Time>());
        REQUIRE_THROWS(YAML::Load("[]").as<Time>());
        REQUIRE_THROWS(YAML::Load("{}").as<Time>());
        REQUIRE_THROWS(YAML::Load("what nanoseconds").as<Time>());
        REQUIRE_THROWS(YAML::Load("29 picoseconds").as<Time>());
        REQUIRE_THROWS(YAML::Load("1e3 centuries").as<Time>());
        REQUIRE_THROWS(YAML::Load("mongodb").as<Time>());
        REQUIRE_THROWS(YAML::Load("1").as<Time>());
        REQUIRE_THROWS(YAML::Load("333").as<Time>());
    }

    SECTION("Barfs on invalid number of spaces") {
        REQUIRE_THROWS(YAML::Load("1  second").as<Time>());
        REQUIRE_THROWS(YAML::Load("1second").as<Time>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Duration"] = Time{30};
        REQUIRE(n["Duration"].as<Time>().count() == 30);
    }
}

TEST_CASE("genny::Integer conversions") {
    SECTION("Can convert to genny::Integer") {
        REQUIRE(YAML::Load("Repeat: 300")["Repeat"].as<Integer>().value == 300);
        REQUIRE(YAML::Load("-1").as<Integer>().value == -1);
        REQUIRE(YAML::Load("0").as<Integer>().value == 0);
        REQUIRE(YAML::Load("1e3").as<Integer>().value == 1000);
        REQUIRE(YAML::Load("10.3e2").as<Integer>().value == 1030);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("1e100000").as<Integer>());
        REQUIRE_THROWS(YAML::Load("1e-3").as<Integer>());
        REQUIRE_THROWS(YAML::Load("foo").as<Integer>());
        REQUIRE_THROWS(YAML::Load("").as<Integer>());
        REQUIRE_THROWS(YAML::Load("-e1").as<Integer>());
        REQUIRE_THROWS(YAML::Load("e").as<Integer>());
        REQUIRE_THROWS(YAML::Load("0.1").as<Integer>());
        REQUIRE_THROWS(YAML::Load("-100.33e-1").as<Integer>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Repeat"] = Integer{30};
        REQUIRE(n["Repeat"].as<Integer>().value == 30);
    }
}

TEST_CASE("genny::Rate conversions") {
    SECTION("Can convert to genny::Rate") {
        REQUIRE(YAML::Load("Rate: 300 per 2 nanoseconds")["Rate"].as<Rate>().operations.value ==
                300);
        REQUIRE(YAML::Load("Rate: 300 per 2 nanoseconds")["Rate"].as<Rate>().per.count() == 2);
        REQUIRE(YAML::Load("-1 per -1 nanosecond").as<Rate>().operations.value == -1);
        REQUIRE(YAML::Load("-1 per -1 nanosecond").as<Rate>().per.count() == -1);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("1 pe 1000 nanoseconds").as<Rate>());
        REQUIRE_THROWS(YAML::Load("per").as<Rate>());
        REQUIRE_THROWS(YAML::Load("nanoseconds per 1").as<Rate>());
        REQUIRE_THROWS(YAML::Load("1per2second").as<Rate>());
        REQUIRE_THROWS(YAML::Load("0per").as<Rate>());
        REQUIRE_THROWS(YAML::Load("xper").as<Rate>());
        REQUIRE_THROWS(YAML::Load("{foo}").as<Rate>());
        REQUIRE_THROWS(YAML::Load("").as<Rate>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Rate"] = Rate{30, 30};
        REQUIRE(n["Rate"].as<Rate>().per.count() == 30);
        REQUIRE(n["Rate"].as<Rate>().operations.value == 30);
    }
}

}  // namespace
}  // namespace genny
