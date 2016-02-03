#include "catch.hpp"
#include "document.hpp"
#include "workload.hpp"

#include <bson.h>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/core.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>

using namespace mwg;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;

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

TEST_CASE("Documents are created", "[documents]") {
    unordered_map<string, bsoncxx::array::value> wvariables;  // workload variables
    unordered_map<string, bsoncxx::array::value> tvariables;  // thread variables

    workload myWorkload;
    auto workloadState = myWorkload.newWorkloadState();
    threadState state(12234, tvariables, wvariables, workloadState, "t", "c");
    bsoncxx::builder::stream::document mydoc{};

    SECTION("Simple bson") {
        unique_ptr<document> doc = makeDoc(YAML::Load("{x : a}"));

        auto view = doc->view(mydoc, state);

        bsoncxx::builder::stream::document refdoc{};
        refdoc << "x"
               << "a";

        viewable_eq_viewable(refdoc, view);
        // Test that the document is bson and has the correct view.
    }


    SECTION("Random Int") {
        // Add a more nested document
        auto doc = makeDoc(YAML::Load(R"yaml(
    type : override
    doc :
        x :
          y : a
        z : 1
    overrides :
        x.y : b
        z   :
            type : randomint
            min : 50
            max : 60
    )yaml"));
        // Test that the document is an override document, and gives the right values.
        auto elem = doc->view(mydoc, state)["z"];
        REQUIRE(elem.type() == bsoncxx::type::k_int64);
        REQUIRE(elem.get_int64().value >= 50);
        REQUIRE(elem.get_int64().value < 60);
    }
    SECTION("Random string") {
        auto doc = makeDoc(YAML::Load(R"yaml(
    type : override
    doc :
      string : a
    overrides :
      string :
        type : randomstring
        length : 15
    )yaml"));

        auto elem = doc->view(mydoc, state)["string"];
        REQUIRE(elem.type() == bsoncxx::type::k_utf8);
        REQUIRE(elem.get_utf8().value.length() == 15);
    }
    SECTION("Date overrides") {
        auto doc = makeDoc(YAML::Load(R"yaml(
            type : override
            doc :
              date : 1
            overrides :
              date :
                type : date)yaml"));
        auto elem = doc->view(mydoc, state)["date"];
        REQUIRE(elem.type() == bsoncxx::type::k_date);
    }
    SECTION("Increment Thread Local") {
        auto doc = makeDoc(YAML::Load(R"yaml(
           type : override
           doc :
              x : 1
           overrides :
              x :
                type : increment
                variable : count)yaml"));
        state.tvariables.insert({"count", bsoncxx::builder::stream::array() << 5 << finalize});
        auto view = doc->view(mydoc, state);
        bsoncxx::builder::stream::document refdoc{};

        // The random number generator is deterministic, so we should get the same string each time
        refdoc << "x" << 5;
        viewable_eq_viewable(refdoc, view);
        bsoncxx::builder::stream::document mydoc2{};
        auto view2 = doc->view(mydoc2, state);
        bsoncxx::builder::stream::document refdoc2{};
        refdoc2 << "x" << 6;
        viewable_eq_viewable(refdoc2, view2);
    }
    SECTION("Increment Workload Var") {
        auto doc = makeDoc(YAML::Load(R"yaml(
           type : override
           doc :
              x : 1
           overrides :
              x :
                type : increment
                variable : count)yaml"));
        state.wvariables.insert({"count", bsoncxx::builder::stream::array() << 5 << finalize});
        auto view = doc->view(mydoc, state);
        bsoncxx::builder::stream::document refdoc{};

        // The random number generator is deterministic, so we should get the same string each time
        refdoc << "x" << 5;
        viewable_eq_viewable(refdoc, view);
        bsoncxx::builder::stream::document mydoc2{};
        auto view2 = doc->view(mydoc2, state);
        bsoncxx::builder::stream::document refdoc2{};
        refdoc2 << "x" << 6;
        viewable_eq_viewable(refdoc2, view2);
    }
}
