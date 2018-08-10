#include "test.h"

#include "../src/generators/generators-private.hh"
#include "../src/generators/parse_util.hh"
#include <gennylib/generators.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/core.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>

using namespace genny::generators;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_document;
using std::mt19937_64;

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

    bsoncxx::builder::stream::document mydoc{};
    mt19937_64 rng;
    rng.seed(269849313357703264);

    SECTION("Simple bson") {
        std::unique_ptr<DocumentGenerator> doc = makeDoc(YAML::Load("{x: a}"), rng);

        auto view = doc->view(mydoc);

        bsoncxx::builder::stream::document refdoc{};
        refdoc << "x"
               << "a";

        viewable_eq_viewable(refdoc, view);
        // Test that the document is bson and has the correct view.
    }
    SECTION("Random Int") {
        auto doc = makeDoc(YAML::Load(R"yaml(
        x :
          y : b
        z : {$randomint: {min: 50, max: 60}}
    )yaml"),
                           rng);
        // Test that the document is an override document, and gives the right values.
        auto elem = doc->view(mydoc)["z"];
        REQUIRE(elem.type() == bsoncxx::type::k_int64);
        REQUIRE(elem.get_int64().value >= 50);
        REQUIRE(elem.get_int64().value < 60);
    }
    SECTION("Random string") {
        auto doc = makeDoc(YAML::Load(R"yaml(
      string: {$randomstring: {length : 15}}
    )yaml"),
                           rng);

        auto elem = doc->view(mydoc)["string"];
        REQUIRE(elem.type() == bsoncxx::type::k_utf8);
        REQUIRE(elem.get_utf8().value.length() == 15);
    }
}

TEST_CASE("Value Generators", "[generators]") {
    mt19937_64 rng;
    rng.seed(269849313357703264);

    SECTION("UseValueGenerator") {
        auto useValueYaml = YAML::Load(R"yaml(
    value: test
)yaml");
        auto valueGenerator = UseValueGenerator(useValueYaml, rng);
        auto result = valueGenerator.generate();
        bsoncxx::builder::stream::array refdoc{};
        refdoc << "test";
        viewable_eq_viewable(refdoc, result.view());
    }
    SECTION("RandomIntGenerator") {
        auto genYaml = YAML::Load(R"yaml(
    min: 50
    max: 60
)yaml");
        auto generator = RandomIntGenerator(genYaml, rng);
        auto result = generator.generate();
        auto elem = result.view()[0];
        REQUIRE(elem.type() == bsoncxx::type::k_int64);
        REQUIRE(elem.get_int64().value >= 50);
        REQUIRE(elem.get_int64().value < 60);
    }
    SECTION("IntOrValue") {
        SECTION("YamlInt") {
            auto genYaml = YAML::Load(R"yaml(
        value: 1
)yaml");
            auto intOrValue = IntOrValue(genYaml, rng);
            REQUIRE(intOrValue.getInt() == 1);
            REQUIRE(intOrValue.getInt() == 1);
        }
    }
    SECTION("RandomString") {
        SECTION("default") {
            auto genYaml = YAML::Load(R"yaml(
)yaml");
            auto generator = RandomStringGenerator(genYaml, rng);
            auto result = generator.generate();
            auto elem = result.view()[0];
            REQUIRE(elem.type() == bsoncxx::type::k_utf8);
            auto str = elem.get_utf8().value;
            INFO("Generated string is " << str);
            REQUIRE(str.length() == 10);
        }
        SECTION("Length") {
            auto genYaml = YAML::Load(R"yaml(
        length: 15
)yaml");
            auto generator = RandomStringGenerator(genYaml, rng);
            auto result = generator.generate();
            auto elem = result.view()[0];
            REQUIRE(elem.type() == bsoncxx::type::k_utf8);
            auto str = elem.get_utf8().value;
            INFO("Generated string is " << str);
            REQUIRE(str.length() == 15);
        }
        SECTION("Alphabet") {
            auto genYaml = YAML::Load(R"yaml(
        alphabet: a
)yaml");
            auto generator = RandomStringGenerator(genYaml, rng);
            auto result = generator.generate();
            auto elem = result.view()[0];
            REQUIRE(elem.type() == bsoncxx::type::k_utf8);
            auto str = elem.get_utf8().value;
            INFO("Generated string is " << str);
            REQUIRE(str.length() == 10);
            REQUIRE(str[0] == 'a');
            REQUIRE(str[1] == 'a');
            REQUIRE(str[9] == 'a');
        }
    }
    SECTION("FastRandomString") {
        SECTION("default") {
            auto genYaml = YAML::Load(R"yaml(
)yaml");
            auto generator = FastRandomStringGenerator(genYaml, rng);
            auto result = generator.generate();
            auto elem = result.view()[0];
            REQUIRE(elem.type() == bsoncxx::type::k_utf8);
            auto str = elem.get_utf8().value;
            INFO("Generated string is " << str);
            REQUIRE(str.length() == 10);
        }
        SECTION("Length") {
            auto genYaml = YAML::Load(R"yaml(
        length: 15
)yaml");
            auto generator = FastRandomStringGenerator(genYaml, rng);
            auto result = generator.generate();
            auto elem = result.view()[0];
            REQUIRE(elem.type() == bsoncxx::type::k_utf8);
            auto str = elem.get_utf8().value;
            INFO("Generated string is " << str);
            REQUIRE(str.length() == 15);
        }
    }
}
