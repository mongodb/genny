#include <iostream>
#include <memory>
#include <string>

#include <bsoncxx/json.hpp>

#include <testlib/yamlTest.hpp>
#include <testlib/yamlToBson.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny {

class ValGenTestCase {
public:
    explicit ValGenTestCase() = default;

    explicit ValGenTestCase(YAML::Node node)
        : _wholeTest{node},
          _name{node["Name"].as<std::string>("No Name")},
          _givenTemplate{node["GivenTemplate"]},
          _thenReturns{node["ThenReturns"]},
          _thenExecuteAndIgnore{node["ThenExecuteAndIgnore"]},
          _expectedExceptionMessage{node["ThenThrows"]} {
        if (!_givenTemplate) {
            std::stringstream msg;
            msg << "Need GivenTemplate in '" << toString(node) << "'";
            BOOST_THROW_EXCEPTION(std::invalid_argument(msg.str()));
        }
        if (_thenReturns && _expectedExceptionMessage) {
            std::stringstream msg;
            msg << "Can't have ThenReturns and ThenThrows in '" << toString(node) << "'";
            BOOST_THROW_EXCEPTION(std::invalid_argument(msg.str()));
        }
        if (_thenExecuteAndIgnore && _expectedExceptionMessage) {
            std::stringstream msg;
            msg << "Can't have ThenExecuteAndIgnore and ThenThrows in '" << toString(node) << "'";
            BOOST_THROW_EXCEPTION(std::invalid_argument(msg.str()));
        }
        if (_thenReturns && _thenExecuteAndIgnore) {
            std::stringstream msg;
            msg << "Can't have ThenReturns and ThenExecuteAndIgnore in '" << toString(node) << "'";
            BOOST_THROW_EXCEPTION(std::invalid_argument(msg.str()));
        }


        if (_thenReturns) {
            if (!_thenReturns.IsSequence()) {
                std::stringstream msg;
                msg << "ThenReturns must be list in '" << toString(node) << "'";
                BOOST_THROW_EXCEPTION(std::invalid_argument(msg.str()));
            }
            _runMode = RunMode::kExpectReturn;
            return;
        }
        if (_thenExecuteAndIgnore) {
            _runMode = RunMode::kIgnoreReturn;
            return;
        }
        if (_expectedExceptionMessage) {
            _runMode = RunMode::kExpectException;
            return;
        }

        std::stringstream msg;
        msg << "Need one of ThenReturns, ThenExecuteAndIgnore or ThenThrows'" << toString(node)
            << "'";
        BOOST_THROW_EXCEPTION(std::invalid_argument(msg.str()));
    }

    static NodeSource toNode(YAML::Node node) {
        return NodeSource{YAML::Dump(node), ""};
    }

    void run() const {
        DYNAMIC_SECTION("DocGenYamlTestCaseRunner " << name()) {
            if (_runMode == RunMode::kExpectException) {
                try {
                    NodeSource ns = toNode(this->_givenTemplate);
                    genny::DefaultRandom rng;

                    auto docGen = genny::DocumentGenerator(ns.root(), GeneratorArgs{rng, 1});
                    docGen();
                    FAIL("Expected exception " << this->_expectedExceptionMessage.as<std::string>()
                                               << " but none occurred");
                } catch (const std::exception& x) {
                    REQUIRE("InvalidValueGeneratorSyntax" ==
                            this->_expectedExceptionMessage.as<std::string>());
                }
                return;
            }
            NodeSource ns = toNode(this->_givenTemplate);
            genny::DefaultRandom rng;
            auto docGen = genny::DocumentGenerator(ns.root(), GeneratorArgs{rng, 2});
            if (_runMode == RunMode::kIgnoreReturn)
                return;
            for (const auto&& nextValue : this->_thenReturns) {
                auto expected = testing::toDocumentBson(nextValue);
                auto actual = docGen();
                // After implementing TIG-2839 uncomment the line below and remove the two lines
                // underneath it as it is a workaround suggested in HELP-21664
                // REQUIRE(toString(expected.view()) == toString(actual.view()));
                auto expectedFix = bsoncxx::from_json(toString(expected.view()));
                INFO("Expected = \n"
                     << toString(expectedFix.view()) << "\nActual = \n"
                     << toString(actual.view()));
                REQUIRE(toString(expectedFix.view()) == toString(actual.view()));
            }
        }
    }

    [[nodiscard]] std::string name() const {
        return _name;
    }

private:
    enum class RunMode {
        kExpectException,
        kExpectReturn,
        kIgnoreReturn,
    };

    RunMode _runMode = RunMode::kExpectException;
    std::string _name;

    // really only "need" _wholeTest but others are here for convenience
    YAML::Node _wholeTest;
    YAML::Node _givenTemplate;
    YAML::Node _thenReturns;
    YAML::Node _thenExecuteAndIgnore;
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
