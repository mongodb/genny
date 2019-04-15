// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vector>

#include <boost/algorithm/string/trim.hpp>
#include <boost/exception/detail/exception_ptr.hpp>
#include <boost/exception/diagnostic_information.hpp>


#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/MongoTestFixture.hpp>
#include <testlib/helpers.hpp>
#include <testlib/yamlTest.hpp>
#include <testlib/yamlToBson.hpp>


namespace genny::testing {

class CrudActorResult : public Result {};


struct CrudActorTestCase {
    using Result = CrudActorResult;

    enum class RunMode { kNormal, kExpectedSetupException, kExpectedRuntimeException };

    static RunMode convertRunMode(YAML::Node tcase) {
        if (tcase["OutcomeData"]) {
            return RunMode::kNormal;
        }
        if (tcase["OutcomeCounts"]) {
            return RunMode::kNormal;
        }
        if (tcase["ExpectedCollectionsExist"]) {
            return RunMode::kNormal;
        }
        if (tcase["Error"]) {
            auto error = tcase["Error"].as<std::string>();
            if (error == "InvalidSyntax") {
                return RunMode::kExpectedSetupException;
            }
            return RunMode::kExpectedRuntimeException;
        }
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid test-case"));
    }

    explicit CrudActorTestCase() = default;

    explicit CrudActorTestCase(YAML::Node node)
        : description{node["Description"].as<std::string>()},
          operations{node["Operations"]},
          runMode{convertRunMode(node)},
          error{node["Error"]},
          tcase{node} {}

    static void assertAfterState(mongocxx::pool::entry& client, YAML::Node tcase, Result& result) {
        if (auto ocd = tcase["OutcomeData"]; ocd) {
            assertOutcomeData(client, ocd, result);
        }
    }

    static void assertOutcomeData(mongocxx::pool::entry& client, YAML::Node ocdata, Result& result) {
        for(auto&& filterYaml : ocdata) {
            auto filter = genny::testing::toDocumentBson(filterYaml);
            long long actual = (*client)["mydb"]["test"].count_documents(filter.view());
            BOOST_LOG_TRIVIAL(info) << "Filter " << bsoncxx::to_json(filter.view()) << " => " << actual;
            // TODO: better error messaging
            result.expectEqual(1, actual);
        }
    }

    static YAML::Node build(YAML::Node operations) {
        YAML::Node config = YAML::Load(R"(
          SchemaVersion: 2018-07-01
          Actors:
          - Name: CrudActor
            Type: CrudActor
            Database: mydb
            RetryStrategy:
              ThrowOnFailure: true
            Phases:
            - Repeat: 1
              Collection: test
          )");
        config["Actors"][0]["Phases"][0]["Operations"] = operations;
        return config;
    }

    Result run() const {
        Result out;
        try {
            auto config = build(operations);
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
            auto client = ah.client();
            dropAllDatabases(client);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });


            if (runMode == RunMode::kExpectedSetupException ||
                runMode == RunMode::kExpectedRuntimeException) {
                out.expectedExceptionButNotThrown();
            } else {
                assertAfterState(client, tcase, out);
            }
        } catch (const std::exception& e) {
            if (runMode == RunMode::kExpectedSetupException ||
                runMode == RunMode::kExpectedRuntimeException) {
                auto actual = boost::trim_copy(std::string{e.what()});
                auto expect = boost::trim_copy(error.as<std::string>());
                out.expectEqual(actual, expect);
            } else {
                auto diagInfo = boost::diagnostic_information(e);
                INFO(description << "CAUGHT " << diagInfo);
                FAIL(diagInfo);
            }
        }
        return out;
    }

    YAML::Node error;
    RunMode runMode;
    std::string description;
    YAML::Node operations;
    YAML::Node tcase;
};

std::ostream& operator<<(std::ostream& out, const std::vector<CrudActorResult>& results) {
    out << std::endl;
    for (auto&& result : results) {
        out << "- Test Case" << std::endl;
        for (auto&& [expect, actual] : result.expectedVsActual()) {
            out << "    - [" << expect << "] != [" << actual << "]" << std::endl;
        }
    }
    return out;
}

}  // namespace genny::testing


namespace YAML {

template <>
struct convert<genny::testing::CrudActorTestCase> {
    static bool decode(YAML::Node node, genny::testing::CrudActorTestCase& out) {
        out = genny::testing::CrudActorTestCase(node);
        return true;
    }
};

}  // namespace YAML


namespace {
TEST_CASE("CrudActor YAML Tests",
          "[standalone][single_node_replset][three_node_replset][CrudActor]") {

    genny::testing::runTestCaseYaml<genny::testing::CrudActorTestCase>(
        "/src/cast_core/test/CrudActorYamlTests.yaml");
}
}  // namespace
