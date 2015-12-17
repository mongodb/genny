#include "catch.hpp"
#include "stats.hpp"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/json.hpp>

using namespace mwg;

template <typename T>
void viewable_eq_viewable(const T& stream, const bsoncxx::document::view& test) {
    using namespace bsoncxx;

    bsoncxx::document::view expected(stream.view());

    auto expect = to_json(expected);
    auto tested = to_json(test);
    INFO("expected = " << expect);
    INFO("basic = " << tested);
    REQUIRE((expect == tested));
}

TEST_CASE("Stats collection", "[stats]") {
    stats testStats;
    testStats.record(std::chrono::microseconds(5000));
    testStats.record(std::chrono::microseconds(3000));
    testStats.record(std::chrono::microseconds(7000));


    SECTION("Basic") {
        INFO("Min is " << testStats.getMin().count());
        INFO("Max is " << testStats.getMax().count());
        INFO("Count is " << testStats.getCount());
        REQUIRE(testStats.getCount() == 3);
        REQUIRE(testStats.getMin().count() == 3000);
        REQUIRE(testStats.getMax().count() == 7000);
        REQUIRE(testStats.getMean().count() == 5000);
        REQUIRE(testStats.getM2().count() == 8000000);
        REQUIRE(testStats.getSampleVariance().count() == 4000000);
        REQUIRE(testStats.getPopVariance().count() == 2666666);
    }

    SECTION("BSON") {
        bsoncxx::builder::stream::document refdoc{};
        refdoc << "count" << 3;
        refdoc << "min" << 3000;
        refdoc << "max" << 7000;
        refdoc << "popStdDev" << 1632;
        refdoc << "mean" << 5000;
        viewable_eq_viewable(refdoc, testStats.getStats(false));
        viewable_eq_viewable(refdoc, testStats.getStats(true));
        bsoncxx::builder::stream::document refdoc2{};
        viewable_eq_viewable(refdoc2, testStats.getStats(false));
    }
}
