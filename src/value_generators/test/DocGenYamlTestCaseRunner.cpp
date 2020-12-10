#include <algorithm>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types/value.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/yamlTest.hpp>
#include <testlib/yamlToBson.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny {

namespace {
genny::DefaultRandom rng;
}  // namespace

class ValGenTestCase {
public:
    explicit ValGenTestCase() = default;

    explicit ValGenTestCase(YAML::Node node)
        : _wholeTest{node},
          _name{node["Name"].as<std::string>("No Name")},
          _givenTemplate{node["GivenTemplate"]},
          _thenReturns{node["ThenReturns"]},
          _expectedExceptionMessage{node["ThenThrows"]} {
        if (!_givenTemplate) {
            std::stringstream msg;
            msg << "Need GivenTemplate in '" << toString(node) << "'";
            throw std::invalid_argument(msg.str());
        }
        if (_thenReturns && _expectedExceptionMessage) {
            std::stringstream msg;
            msg << "Can't have ThenReturns and ThenThrows in '" << toString(node) << "'";
            throw std::invalid_argument(msg.str());
        }
        if (_thenReturns) {
            if (!_thenReturns.IsSequence()) {
                std::stringstream msg;
                msg << "ThenReturns must be list in '" << toString(node) << "'";
                throw std::invalid_argument(msg.str());
            }
            _runMode = RunMode::kExpectReturn;
        } else {
            if (!_expectedExceptionMessage) {
                std::stringstream msg;
                msg << "Need ThenThrows if no ThenReturns in '" << toString(node) << "'";
                throw std::invalid_argument(msg.str());
            }
            _runMode = RunMode::kExpectException;
        }
    }

    static NodeSource toNode(YAML::Node node) {
        return NodeSource{YAML::Dump(node), ""};
    }

    void run() const {
        DYNAMIC_SECTION("DocGenYamlTestCaseRunner " << name()) {
            if (_runMode == RunMode::kExpectException) {
                try {
                    NodeSource ns = toNode(this->_givenTemplate);
                    genny::DocumentGenerator(ns.root(), rng, 1);
                    FAIL("Expected exception " << this->_expectedExceptionMessage.as<std::string>()
                                               << " but none occurred");
                } catch (const std::exception& x) {
                    REQUIRE("InvalidValueGeneratorSyntax" ==
                            this->_expectedExceptionMessage.as<std::string>());
                }
                return;
            }

            NodeSource ns = toNode(this->_givenTemplate);
            auto docGen = genny::DocumentGenerator(ns.root(), rng, 2);
            for (const auto&& nextValue : this->_thenReturns) {
                auto expected = testing::toDocumentBson(nextValue);
                auto actual = docGen();
                REQUIRE(toString(expected.view()) == toString(actual.view()));
            }
        }
    }

    const std::string name() const {
        return _name;
    }

    const YAML::Node givenTemplate() const {
        return _givenTemplate;
    }

private:
    enum class RunMode {
        kExpectException,
        kExpectReturn,
    };

    RunMode _runMode = RunMode::kExpectException;
    std::string _name;

    // really only "need" _wholeTest but others are here for convenience
    YAML::Node _wholeTest;
    YAML::Node _givenTemplate;
    YAML::Node _thenReturns;
    YAML::Node _expectedExceptionMessage;
};

}  // namespace genny


namespace YAML {

template <>
struct convert<genny::ValGenTestCase> {
    static Node encode(genny::ValGenTestCase& rhs) {
        return {};
    }

    static bool decode(const Node& node, genny::ValGenTestCase& rhs) {
        rhs = genny::ValGenTestCase(node);
        return true;
    }
};

}  // namespace YAML

TEMPLATE_TEST_CASE("YAML Tests", "", genny::ValGenTestCase) {
    genny::testing::runTestCaseYaml<TestType>(
        "/src/value_generators/test/DocumentGeneratorTestCases.yml");
}
