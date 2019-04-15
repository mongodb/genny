#include <algorithm>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types/value.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/ActorHelper.hpp>
#include <testlib/findRepoRoot.hpp>
#include <testlib/helpers.hpp>
#include <testlib/yamlTest.hpp>
#include <testlib/yamlToBson.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny {

namespace {
genny::DefaultRandom rng;
}  // namespace


class ValGenTestCaseResult : public genny::testing::Result {
public:
    explicit ValGenTestCaseResult(class ValGenTestCase const* testCase) : _testCase(testCase) {}
    auto testCase() const {
        return _testCase;
    }

private:
    const ValGenTestCase* _testCase;
};

class ValGenTestCase {
public:
    using Result = ValGenTestCaseResult;
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

    ValGenTestCaseResult run() const {
        ValGenTestCaseResult out{this};
        if (_runMode == RunMode::kExpectException) {
            try {
                genny::DocumentGenerator(this->_givenTemplate, rng);
                out.expectedExceptionButNotThrown();
            } catch (const std::exception& x) {
                out.expectEqual("InvalidValueGeneratorSyntax",
                                this->_expectedExceptionMessage.as<std::string>());
            }
            return out;
        }

        auto docGen = genny::DocumentGenerator(this->_givenTemplate, rng);
        for (const auto&& nextValue : this->_thenReturns) {
            auto expected = testing::toDocumentBson(nextValue);
            auto actual = docGen();
            out.expectEqual(expected.view(), actual.view());
        }
        return out;
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

std::ostream& operator<<(std::ostream& out, const std::vector<ValGenTestCaseResult>& results) {
    out << std::endl;
    for (auto&& result : results) {
        out << "- Name: " << result.testCase()->name() << std::endl;
        out << "  GivenTemplate: " << toString(result.testCase()->givenTemplate()) << std::endl;
        out << "  ThenReturns: " << std::endl;
        for (auto&& [expect, actual] : result.expectedVsActual()) {
            out << "    - " << actual << std::endl;
        }
    }
    return out;
}

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
