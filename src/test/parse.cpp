#include "catch.hpp"

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include "parse_util.hpp"
#include "value_generators.hpp"

using namespace mwg;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;
using bsoncxx::to_json;

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
TEST_CASE("isNumber", "[parse]") {
    REQUIRE(isNumber("101") == true);
    REQUIRE(isNumber("0101") == false);
    REQUIRE(isNumber("-101") == true);
    REQUIRE(isNumber("101.2") == true);
    REQUIRE(isNumber("101e10") == true);
    REQUIRE(isNumber("101e+10") == true);
    REQUIRE(isNumber("101e-10") == true);
    REQUIRE(isNumber("true") == false);
    REQUIRE(isNumber("abc") == false);
}
TEST_CASE("isBool", "[parse]") {
    REQUIRE(isBool("true") == true);
    REQUIRE(isBool("false") == true);
    REQUIRE(isBool("abc") == false);
    REQUIRE(isBool("123") == false);
}
TEST_CASE("quoteIfNeeded", "[parse]") {
    REQUIRE(quoteIfNeeded("abc") == "\"abc\"");
    REQUIRE(quoteIfNeeded("\"abc\"") == "\"abc\"");
    REQUIRE(quoteIfNeeded("45") == "45");
    REQUIRE(quoteIfNeeded("\"45\"") == "\"45\"");
    REQUIRE(quoteIfNeeded("\"true\"") == "\"true\"");
    REQUIRE(quoteIfNeeded("true") == "true");
    REQUIRE(quoteIfNeeded("True") == "\"True\"");
}

TEST_CASE("Parse YAML", "[parse]") {
    bsoncxx::builder::stream::array refValue{};
    SECTION("Simple YAML To Value") {
        SECTION("Int") {
            auto testValue = yamlToValue(YAML::Load("3"));
            refValue << 3;
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_int32);
        }
        SECTION("Int String") {
            // Need to escape the quotes in the yaml for this to work
            auto testValue = yamlToValue(YAML::Load("'\"3\"'"));
            refValue << "3";
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_utf8);
        }
        SECTION("Negative Int") {
            auto testValue = yamlToValue(YAML::Load("-3"));
            refValue << -3;
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_int32);
        }
        SECTION("Double") {
            auto testValue = yamlToValue(YAML::Load("-3.02"));
            refValue << -3.02;
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_double);
        }
        SECTION("Exponent") {
            auto testValue = yamlToValue(YAML::Load("-3.02e+2"));
            refValue << -302;
            viewable_eq_viewable(refValue, testValue.view());
        }
        SECTION("True") {
            auto testValue = yamlToValue(YAML::Load("true"));
            refValue << true;
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_bool);
        }
        SECTION("False") {
            auto testValue = yamlToValue(YAML::Load("false"));
            refValue << false;
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_bool);
        }
        SECTION("String") {
            auto testValue = yamlToValue(YAML::Load("string"));
            refValue << "string";
            viewable_eq_viewable(refValue, testValue.view());
            REQUIRE(testValue.view()[0].type() == bsoncxx::type::k_utf8);
        }
    }
    SECTION("Map Parsing") {
        auto testValue = yamlToValue(YAML::Load("{a: 1}"));
        refValue << open_document << "a" << 1 << close_document;
        viewable_eq_viewable(refValue, testValue.view());
    }
    SECTION("Sequence Parsing") {
        auto testValue = yamlToValue(YAML::Load("[1, 2]"));
        refValue << open_array << 1 << 2 << close_array;
        viewable_eq_viewable(refValue, testValue.view());
    }
    SECTION("Map with sequence Parsing") {
        auto testValue = yamlToValue(YAML::Load("{a: [1, 2]}"));
        refValue << open_document << "a" << open_array << 1 << 2 << close_array << close_document;
        viewable_eq_viewable(refValue, testValue.view());
    }

    SECTION("Templating") {
        SECTION("Map Match List1") {
            std::set<std::string> templates;
            templates.insert("$increment");
            templates.insert("$add");
            std::vector<std::tuple<std::string, std::string, YAML::Node>> overrides;
            auto testValue = parseMap(YAML::Load(R"yaml(
x: {$increment: {variable: count, increment: 2, minimum: 0, maximum: 10}}
)yaml"),
                                      templates,
                                      "",
                                      overrides);
            REQUIRE(overrides.size() == 1);
            REQUIRE(std::get<0>(overrides[0]) == "x");
            auto testgen = IncrementGenerator(std::get<2>(overrides[0]));
        }
        SECTION("Map Match List2") {
            std::set<std::string> templates;
            templates.insert("$increment");
            templates.insert("$add");
            std::vector<std::tuple<std::string, std::string, YAML::Node>> overrides;
            auto testValue = parseMap(YAML::Load(R"yaml(
y: 
  a: 
    c: {$add: [1 2 3]}
)yaml"),
                                      templates,
                                      "",
                                      overrides);
            REQUIRE(overrides.size() == 1);
            REQUIRE(std::get<0>(overrides[0]) == "y.a.c");
            auto testgen = AddGenerator(std::get<2>(overrides[0]));
        }
    }
}
