#include <algorithm>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

class YamlTests;

static DefaultRandom rng;

struct YamlTestCase;

class Result {
    std::vector<std::pair<std::string, std::string>> expectedVsActual = {};

public:
    bool pass() {
        return expectedVsActual.empty();
    }
    template <typename E, typename A>
    void expectEqual(E expect, A actual) {
        if (expect != actual) {
            expectedVsActual.emplace(std::to_string(expect), std::to_string(actual));
        }
    }
};


struct YamlTestCase {
    enum class RunMode {
        kExpectException,
        kExpectReturn,
    };

    std::string name;
    YAML::Node givenTemplate;
    YAML::Node thenReturns;
    YAML::Node expectedExceptionMessage;
    RunMode runMode;

    explicit YamlTestCase(YAML::Node node)
        : name{node["Name"].as<std::string>("No Name")},
          givenTemplate{node["GivenTemplate"]},
          thenReturns{node["ThenReturns"]},
          expectedExceptionMessage{node["ThenThrows"]} {
        if (!givenTemplate) {
            throw std::invalid_argument("Need GivenTemplate");
        }
        if (thenReturns && expectedExceptionMessage) {
            throw std::invalid_argument("Can't have ThenReturns and ThenThrows");
        }
        if (thenReturns) {
            if (!thenReturns.IsSequence()) {
                throw std::invalid_argument("ThenReturns must be list");
            }
            runMode = RunMode::kExpectReturn;
        } else {
            if (!expectedExceptionMessage) {
                throw std::invalid_argument("Need ThenThrows if no ThenReturns");
            }
            runMode = RunMode::kExpectException;
        }
    }

    genny::Result run() {
        genny::Result out;
        if (runMode == RunMode::kExpectException) {
            try {
                genny::DocumentGenerator::create(this->givenTemplate, genny::rng);
            } catch (const std::exception& x) {
                out.expectEqual(this->expectedExceptionMessage.as<std::string>(), x.what());
            }
        }
        assert(runMode == RunMode::kExpectReturn);

        auto docGen = genny::DocumentGenerator::create(this->givenTemplate, genny::rng);

        for (const auto&& nextValue : this->thenReturns) {
        }
        return out;
    }
};

struct YamlTests {
    genny::DefaultRandom rng;
    std::vector<std::unique_ptr<YamlTestCase>> cases;
};

}  // namespace genny


namespace YAML {

template <>
struct convert<genny::YamlTestCase> {
    static Node encode(const genny::YamlTestCase& rhs) {
        return {};
    }

    static bool decode(const Node& node, genny::YamlTestCase& rhs) {
        // TODO
        return true;
    }
};

}  // namespace YAML


TEST_CASE("YAML Tests") {
    genny::rng.seed(1234);
}
