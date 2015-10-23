#include "catch.hpp"
#include "document.hpp"

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
    unordered_map<string, atomic_int_least64_t> wvariables;  // workload variables
    unordered_map<string, int64_t> tvariables;               // thread variables

    threadState state(12234, tvariables, wvariables);
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
    )yaml"));
        // Test that the document is an override document, and gives the right values.
        auto view = doc->view(mydoc, state);
        bsoncxx::builder::stream::document refdoc{};

        // The random number generator is deterministic. We should get 24 each time unless we change
        // the seed
        refdoc << "x" << open_document << "y"
               << "b" << close_document << "z" << 24;
        viewable_eq_viewable(refdoc, view);
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

        // Test that we get random strings. Should be reproducible
        auto view = doc->view(mydoc, state);
        bsoncxx::builder::stream::document refdoc{};

        // The random number generator is deterministic, so we should get the same string each time
        refdoc << "string"
               << "YOTBP1XwXID";
        viewable_eq_viewable(refdoc, view);
    }
    SECTION("Date overrides") {
        auto doc = makeDoc(YAML::Load(R"yaml(
            type : override
            doc :
              date : 1
            overrides :
              date :
                type : date)yaml"));
        // Test that we get a date string. Can't force this one right now
        // Test that we get random strings. Should be reproducible
        auto view = doc->view(mydoc, state);
        bsoncxx::builder::stream::document refdoc{};

	REQUIRE(view.length() == 19);
        // The random number generator is deterministic, so we should get the same string each time
        refdoc << "date"
               << "YOTBP1XwXID";
        // should just check that we get a valid date object. Could check that it's current time
        // also.
        //        viewable_eq_viewable(refdoc, view);
    }
    SECTION("Increment") {
        auto doc = makeDoc(YAML::Load(R"yaml(
           type : override
           doc :
              x : 1
           overrides :
              x :
                type : increment
                variable : count)yaml"));
        // Test that we get random strings. Should be reproducible
        state.tvariables["count"] = 5;
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
