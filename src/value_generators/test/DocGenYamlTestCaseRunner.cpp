#include <algorithm>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>
#include <testlib/yamlToBson.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

#include <yaml-cpp/yaml.h>
#include <bsoncxx/types/value.hpp>

namespace genny {

class YamlTests;

static DefaultRandom rng;

struct YamlTestCase;

template<typename T>
std::string toString(const T& t) {
    std::stringstream out;
    out << t;
    return std::string{out.str()};
}

std::string toString(const YAML::Node& node) {
    YAML::Emitter out;
    out << node;
    return std::string{out.c_str()};
}

class Result {
    std::vector<std::pair<std::string, std::string>> expectedVsActual = {};

public:
    bool pass() {
        return expectedVsActual.empty();
    }
    template <typename E, typename A>
    void expectEqual(E expect, A actual) {
        if (expect != actual) {
            expectedVsActual.emplace_back(toString(expect), toString(actual));
        }
    }
};


struct YamlTestCase {
    enum class RunMode {
        kExpectException,
        kExpectReturn,
    };

    std::string name;
    YAML::Node wholeTest;
    YAML::Node givenTemplate;
    YAML::Node thenReturns;
    YAML::Node expectedExceptionMessage;
    RunMode runMode;

    explicit YamlTestCase() = default;

    explicit YamlTestCase(YAML::Node node)
        : wholeTest{node},
          name{node["Name"].as<std::string>("No Name")},
          givenTemplate{node["GivenTemplate"]},
          thenReturns{node["ThenReturns"]},
          expectedExceptionMessage{node["ThenThrows"]} {
        if (!givenTemplate) {
            std::stringstream msg;
            msg << "Need GivenTemplate in " << toString(node);
            throw std::invalid_argument(msg.str());
        }
        if (thenReturns && expectedExceptionMessage) {
            std::stringstream msg;
            msg << "Can't have ThenReturns and ThenThrows in '" << toString(node) << "'";
            throw std::invalid_argument(msg.str());
        }
        if (thenReturns) {
            if (!thenReturns.IsSequence()) {
                std::stringstream msg;
                msg << "ThenReturns must be list in '" << toString(node) << "'";
                throw std::invalid_argument(msg.str());
            }
            runMode = RunMode::kExpectReturn;
        } else {
            if (!expectedExceptionMessage) {
                std::stringstream msg;
                msg << "Need ThenThrows if no ThenReturns in '" << toString(node) << "'";
                throw std::invalid_argument(msg.str());
            }
            runMode = RunMode::kExpectException;
        }
    }

    genny::Result run() {
        genny::Result out;
        if (runMode == RunMode::kExpectException) {
            try {
                genny::DocumentGenerator::create(this->givenTemplate, genny::rng);
                FAIL("Expected exception");
            } catch (const std::exception& x) {
                out.expectEqual(this->expectedExceptionMessage.as<std::string>(), x.what());
                return out;
            }
        }
        if (runMode != RunMode::kExpectReturn) {
            std::stringstream msg;
            msg << "Invalid runMode " << static_cast<int>(runMode);
            throw std::logic_error(msg.str());
        }

        auto docGen = genny::DocumentGenerator::create(this->givenTemplate, genny::rng);

        for (const auto&& nextValue : this->thenReturns) {
            auto expected = testing::toDocumentBson(nextValue);
            auto actual = docGen();

            for(auto&& e : expected.view()) {
                auto ty = static_cast<int>(e.type());
                auto ec = e;
            }

            for(auto&& e : actual.view()) {
                auto ty = static_cast<int>(e.type());
                auto ec = e;
            }

            if (expected.view() != actual.view()) {
                WARN(bsoncxx::to_json(expected) << " != " <<  bsoncxx::to_json(actual));
            }
            REQUIRE(expected.view() == actual.view());
        }
        return out;
    }
};

struct YamlTests {
    explicit YamlTests() { }

    genny::DefaultRandom* rng;
    std::vector<YamlTestCase> cases = {};

    YamlTests(const YamlTests&) = default;

    void run() {
        for(auto& tcase : cases) {
            tcase.run();
        }
    }
};

}  // namespace genny


namespace YAML {

template <>
struct convert<genny::YamlTests> {
    static Node encode(const genny::YamlTests& rhs) {
        return {};
    }

    static bool decode(const Node& node, genny::YamlTests& rhs) {
        rhs.rng = &genny::rng;
        for(auto&& n : node["Cases"]) {
            rhs.cases.push_back(std::move(n.as<genny::YamlTestCase>()));
        }
        return true;
    }
};

template <>
struct convert<genny::YamlTestCase> {
    static Node encode(const genny::YamlTestCase& rhs) {
        return {};
    }

    static bool decode(const Node& node, genny::YamlTestCase& rhs) {
        rhs = genny::YamlTestCase(node);
        return true;
    }
};

}  // namespace YAML


TEST_CASE("YAML Tests") {
    // TODO: findup for path
    try {
        auto yaml = YAML::LoadFile("/Users/rtimmons/Projects/genny/src/value_generators/test/DocumentGeneratorTestCases.yml");
        auto tests = yaml.as<genny::YamlTests>();
        tests.run();
    } catch(const std::exception& ex) {
        WARN(ex.what());
        throw;
    }
}
