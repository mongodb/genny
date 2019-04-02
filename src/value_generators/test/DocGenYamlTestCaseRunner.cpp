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
#include <testlib/yamlToBson.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny {

namespace {

genny::DefaultRandom rng;

std::string toString(const std::string& str) {
    return str;
}

std::string toString(const bsoncxx::document::view_or_value& t) {
    return bsoncxx::to_json(t, bsoncxx::ExtendedJsonMode::k_canonical);
}

std::string toString(const YAML::Node& node) {
    YAML::Emitter out;
    out << node;
    return std::string{out.c_str()};
}

}  // namespace


class YamlTests;
struct YamlTestCase;


class Result {
public:
    explicit Result(const YamlTestCase& testCase) : _testCase{testCase} {}

    bool pass() const {
        return _expectedVsActual.empty() && !_expectedExceptionButNotThrown;
    }

    template <typename E, typename A>
    void expectEqual(E expect, A actual) {
        if (expect != actual) {
            _expectedVsActual.emplace_back(toString(expect), toString(actual));
        }
    }

    void expectedExceptionButNotThrown() {
        _expectedExceptionButNotThrown = true;
    }

    const std::vector<std::pair<std::string, std::string>>& expectedVsActual() const {
        return _expectedVsActual;
    }

    const YamlTestCase& testCase() const {
        return _testCase;
    }

private:
    const YamlTestCase& _testCase;
    std::vector<std::pair<std::string, std::string>> _expectedVsActual;
    bool _expectedExceptionButNotThrown = false;
};


class YamlTestCase {
public:
    explicit YamlTestCase() = default;

    explicit YamlTestCase(YAML::Node node)
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

    genny::Result run() const {
        genny::Result out{*this};
        if (_runMode == RunMode::kExpectException) {
            try {
                genny::DocumentGenerator::create(this->_givenTemplate, rng);
                out.expectedExceptionButNotThrown();
            } catch (const std::exception& x) {
                out.expectEqual("InvalidValueGeneratorSyntax",
                                this->_expectedExceptionMessage.as<std::string>());
            }
            return out;
        }

        auto docGen = genny::DocumentGenerator::create(this->_givenTemplate, rng);
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


class YamlTests {
public:
    explicit YamlTests() = default;
    YamlTests(const YamlTests&) = default;

    explicit YamlTests(const YAML::Node& node) {
        for (auto&& n : node["Cases"]) {
            _cases.push_back(std::move(n.as<genny::YamlTestCase>()));
        }
    }

    std::vector<Result> run() const {
        std::vector<Result> out{};
        for (auto& tcase : _cases) {
            auto result = tcase.run();
            if (!result.pass()) {
                out.push_back(std::move(result));
            }
        }
        return out;
    }

private:
    std::vector<YamlTestCase> _cases;
};

std::ostream& operator<<(std::ostream& out, const std::vector<Result>& results) {
    out << std::endl;
    for (auto&& result : results) {
        out << "- Name: " << result.testCase().name() << std::endl;
        out << "  GivenTemplate: " << toString(result.testCase().givenTemplate()) << std::endl;
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
struct convert<genny::YamlTests> {
    static Node encode(const genny::YamlTests& rhs) {
        return {};
    }

    static bool decode(const Node& node, genny::YamlTests& rhs) {
        rhs = genny::YamlTests(node);
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


namespace {

TEST_CASE("YAML Tests") {
    try {
        const auto file =
            genny::findRepoRoot() + "/src/value_generators/test/DocumentGeneratorTestCases.yml";
        const auto yaml = YAML::LoadFile(file);
        auto tests = yaml.as<genny::YamlTests>();
        std::vector<genny::Result> results = tests.run();
        if (!results.empty()) {
            std::stringstream msg;
            msg << results;
            WARN(msg.str());
        }
        REQUIRE(results.empty());
    } catch (const std::exception& ex) {
        WARN(ex.what());
        throw;
    }
}

}  // namespace