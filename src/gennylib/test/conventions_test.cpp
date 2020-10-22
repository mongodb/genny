// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <cmath>

#include <gennylib/Node.hpp>
#include <gennylib/conventions.hpp>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>


namespace genny {
namespace {
using namespace std::chrono;

TEST_CASE("Conventions used by PhaseLoop") {
    NodeSource ns(R"(
    SchemaVersion: 2018-07-01
    Database: test
    Actors:
    - Name: MetricsNameTest
      Type: HelloWorld
      Threads: 1
      Phases:
      - Repeat: 1
    )",
                  "");
    auto& yaml = ns.root();
    auto& phaseContext = yaml["Actors"][0]["Phases"][0];
    // Test of the test
    REQUIRE(phaseContext);

    REQUIRE(phaseContext["Nop"].maybe<bool>().value_or(false) == false);

    // from `explicit IterationChecker(PhaseContext& phaseContext)` ctor
    REQUIRE(phaseContext["Duration"].maybe<TimeSpec>() == std::nullopt);
    REQUIRE(*(phaseContext["Repeat"].maybe<IntegerSpec>()) == IntegerSpec{1});
    REQUIRE(phaseContext["SleepBefore"].maybe<TimeSpec>().value_or(TimeSpec{33}) == TimeSpec{33});
    REQUIRE(phaseContext["SleepAfter"].maybe<TimeSpec>().value_or(TimeSpec{33}) == TimeSpec{33});
    REQUIRE(phaseContext["Rate"].maybe<RateSpec>() == std::nullopt);
    REQUIRE(phaseContext["RateLimiterName"].maybe<std::string>().value_or("defaultRateLimiter") ==
            "defaultRateLimiter");
};

TEST_CASE("genny::TimeSpec conversions") {
    SECTION("Can convert to genny::TimeSpec") {
        REQUIRE(YAML::Load("D: 3 seconds")["D"].as<TimeSpec>().count() == 3 * std::pow(10, 9));
        REQUIRE(YAML::Load("0 second").as<TimeSpec>().count() == 0);
        REQUIRE(YAML::Load("20 millisecond").as<TimeSpec>().count() == 20 * std::pow(10, 6));
        REQUIRE(YAML::Load("33 microsecond").as<TimeSpec>().count() == 33 * std::pow(10, 3));
        REQUIRE(YAML::Load("2e3 microseconds").as<TimeSpec>().count() == 2000 * std::pow(10, 3));
        REQUIRE(YAML::Load("10.3e2 nanoseconds").as<TimeSpec>().count() == 1030);
        REQUIRE(YAML::Load("3 hour").as<TimeSpec>().count() == 3 * 3600 * std::pow(10, 9));
        REQUIRE(YAML::Load("2 minutes").as<TimeSpec>().count() == 2 * 60 * std::pow(10, 9));
    }

    SECTION("Overlooks small typos") {
        REQUIRE(YAML::Load("D: 3 secondsasdfadsf     ")["D"].as<TimeSpec>().count() ==
                3 * std::pow(10, 9));
    }

    SECTION("Barfs on unknown types") {
        REQUIRE_THROWS(YAML::Load("-1 nanosecond").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("foo").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("[1,2,3]").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("[]").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("{}").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("what nanoseconds").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("29 picoseconds").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("1e3 centuries").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("mongodb").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("1").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("333").as<TimeSpec>());
    }

    SECTION("Barfs on invalid number of spaces") {
        REQUIRE_THROWS(YAML::Load("1  second").as<TimeSpec>());
        REQUIRE_THROWS(YAML::Load("1second").as<TimeSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Duration"] = TimeSpec{30};
        REQUIRE(n["Duration"].as<TimeSpec>().count() == 30);
    }
}

TEST_CASE("genny::IntegerSpec conversions") {
    SECTION("Can convert to genny::IntegerSpec") {
        REQUIRE(YAML::Load("Repeat: 300")["Repeat"].as<IntegerSpec>().value == 300);
        REQUIRE(YAML::Load("0").as<IntegerSpec>().value == 0);
        REQUIRE(YAML::Load("1e3").as<IntegerSpec>().value == 1000);
        REQUIRE(YAML::Load("10.3e2").as<IntegerSpec>().value == 1030);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("-1").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("1e100000").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("1e-3").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("foo").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("-e1").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("e").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("0.1").as<IntegerSpec>());
        REQUIRE_THROWS(YAML::Load("-100.33e-1").as<IntegerSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Repeat"] = IntegerSpec{30};
        REQUIRE(n["Repeat"].as<IntegerSpec>().value == 30);
    }
}

TEST_CASE("genny::BaseRateSpec conversions") {
    SECTION("Can convert to genny::BaseRateSpec") {
        REQUIRE(YAML::Load("GlobalRate: 300 per 2 nanoseconds")["GlobalRate"]
                    .as<BaseRateSpec>()
                    .operations == 300);
        REQUIRE(YAML::Load("GlobalRate: 300 per 2 nanoseconds")["GlobalRate"]
                    .as<BaseRateSpec>()
                    .per.count() == 2);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("-1 per -1 nanosecond").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("-1 per -1 nanosecond").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("1 pe 1000 nanoseconds").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("per").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("nanoseconds per 1").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("1per2second").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("0per").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("xper").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("{foo}").as<BaseRateSpec>());
        REQUIRE_THROWS(YAML::Load("").as<BaseRateSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["GlobalRate"] = BaseRateSpec{20, 30};
        REQUIRE(n["GlobalRate"].as<BaseRateSpec>().per.count() == 20);
        REQUIRE(n["GlobalRate"].as<BaseRateSpec>().operations == 30);
    }
}

TEST_CASE("genny::PercentileRateSpec conversions") {
    SECTION("Can convert to genny::PercentileRateSpec") {
        REQUIRE(YAML::Load("GlobalRate: 50%")["GlobalRate"].as<PercentileRateSpec>().percent == 50);
        REQUIRE(YAML::Load("GlobalRate: 78%")["GlobalRate"].as<PercentileRateSpec>().percent == 78);
        REQUIRE(YAML::Load("GlobalRate: 5%")["GlobalRate"].as<PercentileRateSpec>().percent == 5);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("-1%").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("2899").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("300 per 2 nanoseconds").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("%").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("%15").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("28.999%").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("28.999%").as<PercentileRateSpec>());
        REQUIRE_THROWS(YAML::Load("").as<PercentileRateSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["GlobalRate"] = PercentileRateSpec{25};
        REQUIRE(n["GlobalRate"].as<PercentileRateSpec>().percent == 25);
    }
}

TEST_CASE("genny::RateSpec conversions") {
    SECTION("Can convert to genny::RateSpec") {
        REQUIRE(YAML::Load("GlobalRate: 25 per 5 seconds")["GlobalRate"]
                    .as<RateSpec>()
                    .getBaseSpec()
                    ->operations == 25);
        REQUIRE(YAML::Load("GlobalRate: 25 per 5 seconds")["GlobalRate"]
                    .as<RateSpec>()
                    .getBaseSpec()
                    ->per.count() == 5000000000);
        REQUIRE_FALSE(YAML::Load("GlobalRate: 25 per 5 seconds")["GlobalRate"]
                          .as<RateSpec>()
                          .getPercentileSpec());

        REQUIRE(YAML::Load("GlobalRate: 30%")["GlobalRate"]
                    .as<RateSpec>()
                    .getPercentileSpec()
                    ->percent == 30);
        REQUIRE_FALSE(YAML::Load("GlobalRate: 30%")["GlobalRate"].as<RateSpec>().getBaseSpec());
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("p%er").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("25 nanoseconds per 1").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("46%28").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("{499}").as<RateSpec>());
        REQUIRE_THROWS(YAML::Load("").as<RateSpec>());
    }

    SECTION("Can encode") {
        auto base = BaseRateSpec{20, 30};
        YAML::Node n1;
        n1["GlobalRate"] = RateSpec{base};
        REQUIRE(n1["GlobalRate"].as<RateSpec>().getBaseSpec()->per.count() == 20);
        REQUIRE(n1["GlobalRate"].as<RateSpec>().getBaseSpec()->operations == 30);

        auto percentile = PercentileRateSpec{75};
        YAML::Node n2;
        n2["GlobalRate"] = RateSpec{percentile};
        REQUIRE(n2["GlobalRate"].as<RateSpec>().getPercentileSpec()->percent == 75);
    }
}


TEST_CASE("genny::PhaseRangeSpec conversions") {
    SECTION("Can convert to genny::PhaseRangeSpec") {
        auto yaml = YAML::Load("Phase: 0..20");
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().start == 0);
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().end == 20);

        yaml = YAML::Load("Phase: 2..2");
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().start == 2);
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().end == 2);

        yaml = YAML::Load("Phase: 0..1e2");
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().start == 0);
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().end == 100);

        yaml = YAML::Load("Phase: 10 .. 1e2");
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().start == 10);
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().end == 100);

        yaml = YAML::Load("Phase: 12");
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().start == 12);
        REQUIRE(yaml["Phase"].as<PhaseRangeSpec>().end == 12);
    }

    SECTION("Barfs on invalid values") {
        REQUIRE_THROWS(YAML::Load("0....20").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("0.1").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("-1..1").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("0abc..20").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("0abc .. 20").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("10..4294967296").as<PhaseRangeSpec>());  // uint_max + 1
        REQUIRE_THROWS(YAML::Load("4294967296..4294967296").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("20..25abc").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("-10").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("12abc").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("{foo}").as<PhaseRangeSpec>());
        REQUIRE_THROWS(YAML::Load("").as<PhaseRangeSpec>());
    }

    SECTION("Can encode") {
        YAML::Node n;
        n["Phase"] = PhaseRangeSpec{0, 10};
        REQUIRE(n["Phase"].as<PhaseRangeSpec>().start == 0);
        REQUIRE(n["Phase"].as<PhaseRangeSpec>().end == 10);
    }
}

}  // namespace
}  // namespace genny
