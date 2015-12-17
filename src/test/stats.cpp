#include "catch.hpp"
#include "stats.hpp"

using namespace mwg;

TEST_CASE("Stats collection", "[stats]") {
    stats testStats;
    testStats.record(std::chrono::microseconds(5));
    testStats.record(std::chrono::microseconds(3));
    testStats.record(std::chrono::microseconds(7));

    INFO("Min is " << testStats.getMin().count());
    INFO("Max is " << testStats.getMax().count());
    INFO("Count is " << testStats.getCount());
    REQUIRE(testStats.getCount() == 3);
    REQUIRE(testStats.getMin().count() == 3);
    REQUIRE(testStats.getMax().count() == 7);
    REQUIRE(testStats.getAvg().count() == 5);
}
