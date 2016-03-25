#include "catch.hpp"

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/json.hpp>
#include <unordered_map>

#include "concatenate_generator.hpp"
#include "int_or_value.hpp"
#include "value_generators.hpp"
#include "workload.hpp"

using namespace mwg;
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

TEST_CASE("Value Generaotrs", "[generators]") {
    std::unordered_map<std::string, bsoncxx::array::value> wvariables;  // workload variables
    std::unordered_map<std::string, bsoncxx::array::value> tvariables;  // thread variables

    workload myWorkload;
    auto workloadState = myWorkload.newWorkloadState();
    auto state = shared_ptr<threadState>(
        new threadState(12234, tvariables, wvariables, workloadState, "t", "c"));
    state->tvariables.insert({"tvar", bsoncxx::builder::stream::array() << 1 << finalize});
    state->wvariables.insert({"wvar", bsoncxx::builder::stream::array() << 2 << finalize});

    SECTION("UseVarGenerator") {
        SECTION("tvariable") {
            auto useVarYaml = YAML::Load(R"yaml(
    variable: tvar
)yaml");
            auto varGenerator = UseVarGenerator(useVarYaml);
            auto result = varGenerator.generate(*state);
            REQUIRE(result.view()[0].get_int32().value == 1);
        }
        SECTION("wvariable") {
            auto useVarYaml = YAML::Load(R"yaml(
    variable: wvar
)yaml");
            auto varGenerator = UseVarGenerator(useVarYaml);
            auto result = varGenerator.generate(*state);
            REQUIRE(result.view()[0].get_int32().value == 2);
        }
        SECTION("DBName") {
            auto useVarYaml = YAML::Load(R"yaml(
    variable: DBName
)yaml");
            auto varGenerator = UseVarGenerator(useVarYaml);
            auto result = varGenerator.generate(*state);
            auto expected = "t";
            auto actual = result.view()[0].get_utf8().value;
            INFO("Expected = \"" << expected << "\"");
            INFO("Got      = \"" << actual << "\"");
            REQUIRE(actual.compare(expected) == 0);
        }
        SECTION("CollectionName") {
            auto useVarYaml = YAML::Load(R"yaml(
    variable: CollectionName
)yaml");
            auto varGenerator = UseVarGenerator(useVarYaml);
            auto result = varGenerator.generate(*state);
            auto expected = "c";
            auto actual = result.view()[0].get_utf8().value;
            INFO("Expected = \"" << expected << "\"");
            INFO("Got      = \"" << actual << "\"");
            REQUIRE(actual.compare(expected) == 0);
        }
    }
    SECTION("UseValueGenerator") {
        auto useValueYaml = YAML::Load(R"yaml(
    value: test
)yaml");
        auto valueGenerator = UseValueGenerator(useValueYaml);
        auto result = valueGenerator.generate(*state);
        bsoncxx::builder::stream::array refdoc{};
        refdoc << "test";
        viewable_eq_viewable(refdoc, result.view());
    }

    SECTION("IncrementGenerator") {
        SECTION("tvariable") {
            auto incYaml = YAML::Load(R"yaml(
    variable: tvar
)yaml");
            auto incGenerator = IncrementGenerator(incYaml);
            auto result = incGenerator.generate(*state);
            REQUIRE(result.view()[0].get_int32().value == 1);
            result = incGenerator.generate(*state);
            REQUIRE(result.view()[0].get_int32().value == 2);
        }
        SECTION("tvariable") {
            auto incYaml = YAML::Load(R"yaml(
    variable: wvar
)yaml");
            auto incGenerator = IncrementGenerator(incYaml);
            auto result = incGenerator.generate(*state);
            REQUIRE(result.view()[0].get_int32().value == 2);
            result = incGenerator.generate(*state);
            REQUIRE(result.view()[0].get_int32().value == 3);
        }
    }

    SECTION("DateGenerator") {
        auto genYaml = YAML::Load(R"yaml(
)yaml");
        auto generator = DateGenerator(genYaml);
        auto result = generator.generate(*state);
        REQUIRE(result.view()[0].type() == bsoncxx::type::k_date);
    }

    SECTION("RandomIntGenerator") {
        auto genYaml = YAML::Load(R"yaml(
    min: 50
    max: 60
)yaml");
        auto generator = RandomIntGenerator(genYaml);
        auto result = generator.generate(*state);
        auto elem = result.view()[0];
        REQUIRE(elem.type() == bsoncxx::type::k_int64);
        REQUIRE(elem.get_int64().value >= 50);
        REQUIRE(elem.get_int64().value < 60);
    }
    SECTION("MultipleGenerator") {
        auto genYaml = YAML::Load(R"yaml(
      variable: wvar
      factor: 4
)yaml");
        auto generator = MultiplyGenerator(genYaml);
        auto result = generator.generate(*state);
        auto elem = result.view()[0];
        REQUIRE(elem.type() == bsoncxx::type::k_int32);
        REQUIRE(elem.get_int32().value == 8);
        REQUIRE(generator.generateInt(*state) == 8);
    }
    SECTION("RandomString") {
        SECTION("default") {
            auto genYaml = YAML::Load(R"yaml(
)yaml");
            auto generator = RandomStringGenerator(genYaml);
            auto result = generator.generate(*state);
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
            auto generator = RandomStringGenerator(genYaml);
            auto result = generator.generate(*state);
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
            auto generator = RandomStringGenerator(genYaml);
            auto result = generator.generate(*state);
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
    SECTION("UseResult") {
        auto genYaml = YAML::Load(R"yaml(
)yaml");
        auto generator = UseResultGenerator(genYaml);
        state->result = bsoncxx::builder::stream::array() << 5 << finalize;
        auto result = generator.generate(*state);
        REQUIRE(result.view()[0].get_int32().value == 5);
    }
    SECTION("Choose1") {
        auto genYaml = YAML::Load(R"yaml(
    choices:
       - thingA
)yaml");
        auto generator = ChooseGenerator(genYaml);
        auto result = generator.generate(*state);
        auto actual = result.view()[0].get_utf8().value;
        INFO("Expected thingA");
        INFO("Got" << actual);
        REQUIRE(actual.compare("thingA") == 0);
    }
    SECTION("Choose2") {
        auto genYaml = YAML::Load(R"yaml(
    choices:
       - thingA
       - thingB
)yaml");
        auto generator = ChooseGenerator(genYaml);
        auto result = generator.generate(*state);
        auto actual = result.view()[0].get_utf8().value;
        INFO("Expected thingA or thingB");
        INFO("Got" << actual);
        auto test = actual.compare("thingA") == 0 || actual.compare("thingB") == 0;
        REQUIRE(test);
    }
    SECTION("IntOrValue") {
        SECTION("YamlInt") {
            auto genYaml = YAML::Load(R"yaml(
        value: 1
)yaml");
            auto intOrValue = IntOrValue(genYaml);
            REQUIRE(intOrValue.getInt(*state) == 1);
            REQUIRE(intOrValue.getInt(*state) == 1);
        }
    }
    SECTION("Concatenate") {
        auto genYaml = YAML::Load(R"yaml(
        parts:
          - A
          - 1
          - type: randomint
            min: 5
            max: 5
)yaml");
        auto generator = ConcatenateGenerator(genYaml);
        auto result = generator.generate(*state);
        auto elem = result.view()[0];
        REQUIRE(elem.type() == bsoncxx::type::k_utf8);
        auto str = elem.get_utf8().value;
        INFO("Generated string is " << str);
        REQUIRE(str.length() == 3);
        REQUIRE(str.compare("A15") == 0);
    }
}
