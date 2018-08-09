#include "test.h"

#include <gennylib/generators.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/core.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/types/value.hpp>

using namespace genny;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_document;

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
        unique_ptr<document> doc = makeDoc(YAML::Load("{x: a}"));

        auto view = doc->view(mydoc, rng);

        bsoncxx::builder::stream::document refdoc{};
        refdoc << "x"
               << "a";

        viewable_eq_viewable(refdoc, view);
        // Test that the document is bson and has the correct view.
    }
}

TEST_CASE("Value Generators", "[generators]") {
    mt19937_64 rng;
    rng.seed(269849313357703264);

    SECTION("UseValueGenerator") {
        auto useValueYaml = YAML::Load(R"yaml(
    value: test
)yaml");
        auto valueGenerator = UseValueGenerator(useValueYaml);
        auto result = valueGenerator.generate(rng);
        bsoncxx::builder::stream::array refdoc{};
        refdoc << "test";
        viewable_eq_viewable(refdoc, result.view());
    }
    SECTION("RandomIntGenerator") {
        auto genYaml = YAML::Load(R"yaml(
    min: 50
    max: 60
)yaml");
        auto generator = RandomIntGenerator(genYaml);
        auto result = generator.generate(rng);
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
            auto intOrValue = IntOrValue(genYaml);
            REQUIRE(intOrValue.getInt(rng) == 1);
            REQUIRE(intOrValue.getInt(rng) == 1);
        }
    }
}
