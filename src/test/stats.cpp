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
    testStats.record(std::chrono::microseconds(5));
    testStats.record(std::chrono::microseconds(3));
    testStats.record(std::chrono::microseconds(7));


    SECTION("Basic") {
        INFO("Min is " << testStats.getMin().count());
        INFO("Max is " << testStats.getMax().count());
        INFO("Count is " << testStats.getCount());
        REQUIRE(testStats.getCount() == 3);
        REQUIRE(testStats.getMin().count() == 3);
        REQUIRE(testStats.getMax().count() == 7);
        REQUIRE(testStats.getAvg().count() == 5);
    }

    SECTION("BSON") {
        bsoncxx::builder::stream::document refdoc{};
        refdoc << "count" << 3;
        refdoc << "min" << 3;
        refdoc << "max" << 7;
        refdoc << "average" << 5;
        viewable_eq_viewable(refdoc, testStats.getStats());
    }
}
