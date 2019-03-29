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

#include <fstream>
#include <mutex>
#include <set>
#include <stdio.h>
#include <streambuf>
#include <string>

#include <boost/exception/exception.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <driver/DefaultDriver.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <testlib/ActorHelper.hpp>

#include <testlib/helpers.hpp>
#include <testlib/findRepoRoot.hpp>


using namespace genny::driver;

namespace {

std::string readFile(const std::string& fileName) {
    std::ifstream t(fileName);
    if (!t) {
        return "";
    }
    std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    return str;
}

std::string metricsContents(DefaultDriver::ProgramOptions& options) {
    return readFile(options.metricsOutputFileName);
}

// Ideally this would use std::filesystem::file_size but
// <filesystem> isn't yet available on all the platforms
// we support (I'm looking at you, Apple Clang).
bool hasMetrics(DefaultDriver::ProgramOptions& options) {
    return !metricsContents(options).empty();
}

class SomeException : public virtual boost::exception, public virtual std::exception {};

class StaticFailsInfo {
public:
    void didReachPhase(int phase) {
        std::lock_guard<std::mutex> lock(this->mutex);
        phaseCalls.insert(phase);
    }

    const std::multiset<genny::PhaseNumber>& reachedPhases() const {
        return this->phaseCalls;
    }

    void clear() {
        this->phaseCalls.clear();
    }

private:
    std::multiset<genny::PhaseNumber> phaseCalls;
    std::mutex mutex;
};

struct Fails : public genny::Actor {
    struct PhaseConfig {
        std::string mode;
        PhaseConfig(genny::PhaseContext& phaseContext)
            : mode{phaseContext.get<std::string>("Mode")} {}
    };
    genny::PhaseLoop<PhaseConfig> loop;
    static StaticFailsInfo state;

    explicit Fails(genny::ActorContext& ctx) : Actor(ctx), loop{ctx} {}

    static std::string_view defaultName() {
        return "Fails";
    }
    void run() override {
        for (auto&& config : loop) {
            for (auto&& _ : config) {
                state.didReachPhase(config.phaseNumber());

                if (config->mode == "NoException") {
                    continue;
                } else if (config->mode == "BoostException") {
                    auto x = SomeException{};
                    BOOST_THROW_EXCEPTION(x);
                } else if (config->mode == "StdException") {
                    throw std::exception{};
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Bad Mode " << config->mode;
                    std::terminate();
                }
            }
        }
    }
};

namespace {
auto registerFails = genny::Cast::registerDefault<Fails>();
}  // namespace

// initialize static member of Fails
StaticFailsInfo Fails::state = {};


DefaultDriver::ProgramOptions create(const std::string& yaml) {
    boost::filesystem::path ph = boost::filesystem::unique_path();
    boost::filesystem::create_directories(ph);
    auto metricsOutputFileName = (ph / "metrics.csv").string();

    DefaultDriver::ProgramOptions opts;

    opts.metricsFormat = "csv";
    opts.metricsOutputFileName = metricsOutputFileName;
    opts.mongoUri = "mongodb://localhost:27017";
    opts.workloadSourceType = DefaultDriver::ProgramOptions::YamlSource::kString;
    opts.workloadSource = yaml;

    return opts;
}

std::pair<DefaultDriver::OutcomeCode, DefaultDriver::ProgramOptions> outcome(
    const std::string& yaml) {
    Fails::state.clear();

    DefaultDriver driver;
    auto opts = create(yaml);
    remove(opts.metricsOutputFileName.c_str());
    return {driver.run(opts), opts};
}

}  // namespace


TEST_CASE("Various Actor Behaviors") {
    boost::filesystem::current_path(genny::findRepoRoot());

    SECTION("Normal Execution") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
          - Mode: NoException
            Repeat: 1
        )");
        REQUIRE((code == DefaultDriver::OutcomeCode::kSuccess));
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Normal Execution: Two Repeat") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
          - Mode: NoException
            Repeat: 2
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kSuccess);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0, 0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Std Exception: Two Repeat") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
          - Mode: StdException
            Repeat: 2
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kStandardException);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Boost Exception") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
            - Repeat: 1
              Mode: BoostException
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kBoostException);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Std Exception") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
            - Repeat: 1
              Mode: StdException
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kStandardException);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }


    SECTION("Boost Exception in phase 2 by 2 threads") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 2
          Phases:
            - Repeat: 1
              Mode: NoException
            - Repeat: 1
              Mode: BoostException
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kBoostException);
        REQUIRE((Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0, 0, 1, 1} ||
                 Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0, 0, 1}));
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Exception prevents other Phases") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
            - Repeat: 1
              Mode: BoostException
            - Repeat: 1
              Mode: NoException
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kBoostException);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("200 Actors simultaneously throw") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 200
          Phases:
            - Repeat: 1
              Mode: StdException
        )");

        REQUIRE(code == DefaultDriver::OutcomeCode::kStandardException);

        REQUIRE(Fails::state.reachedPhases().size() > 0);
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Two Actors simultaneously throw different exceptions") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
            - Repeat: 1
              Mode: BoostException
        - Type: Fails
          Threads: 1
          Phases:
            - Repeat: 1
              Mode: StdException
        )");

        // we set the outcome code atomically so
        // either the boost::exception or the
        // std::exception may get thrown/handled first
        REQUIRE((code == DefaultDriver::OutcomeCode::kStandardException ||
                 code == DefaultDriver::OutcomeCode::kBoostException));

        REQUIRE((Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0, 0} ||
                 Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0}));
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Boost exception by two threads") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 2
            Phases:
              - Repeat: 1
                Mode: BoostException
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kBoostException);
        REQUIRE((Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0, 0} ||
                 Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0}));
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Load External Config Default Parameter") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 1
            Phases:
            - ExternalPhaseConfig:
                Path: "src/phase_configs/self_tests/Good.yml"

        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kSuccess);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Load External Config Default Parameter") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 1
            Phases:
            - ExternalPhaseConfig:
                Path: "src/phase_configs/self_tests/GoodWithKey.yml"
                Key: ForSelfTest
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kSuccess);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Load External Config Override Parameter") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 1
            Phases:
            - ExternalPhaseConfig:
                Path: "src/phase_configs/self_tests/Good.yml"
                Parameters:
                  Repeat: 2

        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kSuccess);
        REQUIRE(Fails::state.reachedPhases() == std::multiset<genny::PhaseNumber>{0, 0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Load Bad External State 1") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 1
            Phases:
            - ExternalPhaseConfig:
                Path: "src/phase_configs/self_tests/MissingAllFields.yml"
                Parameters:
                  Repeat: 2

        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kInternalException);
        REQUIRE(!hasMetrics(opts));
    }

    SECTION("Load Bad External State 2") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 1
            Phases:
            - ExternalPhaseConfig:
                Path: "src/phase_configs/self_tests/MissingDefault.yml"
                Parameters:
                  Repeat: 2

        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kInternalException);
        REQUIRE(!hasMetrics(opts));
    }

    SECTION("Load Bad External State 3") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
          - Type: Fails
            Threads: 1
            Phases:
            - ExternalPhaseConfig:
                Path: "src/phase_configs/self_tests/MissingName.yml"
                Parameters:
                  Repeat: 2

        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kInternalException);
        REQUIRE(!hasMetrics(opts));
    }
}
