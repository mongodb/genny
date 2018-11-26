#include <fstream>
#include <mutex>
#include <set>
#include <stdio.h>
#include <streambuf>
#include <string>

#include <boost/exception/exception.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <DefaultDriver.hpp>
#include <helpers.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>


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

template <class F>
auto onActorContext(F&& callback) {
    return [&](genny::ActorContext& context) {
        genny::ActorVector vec;
        callback(context, vec);
        return vec;
    };
}


class SomeException : public virtual boost::exception, public virtual std::exception {};

struct Fails : public genny::Actor {
    struct PhaseConfig {
        std::string mode;
        PhaseConfig(genny::PhaseContext& phaseContext)
            : mode{phaseContext.get<std::string>("Mode")} {}
    };
    genny::PhaseLoop<PhaseConfig> loop;
    static std::multiset<int> phaseCalls;
    static std::mutex mutex;

    explicit Fails(genny::ActorContext& ctx) : loop{ctx} {}

    void run() override {
        for (auto&& [phase, config] : loop) {
            for (auto&& _ : config) {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    Fails::phaseCalls.insert(phase);
                }

                if (config->mode == "None") {
                    continue;
                } else if (config->mode == "BoostException") {
                    auto x = SomeException{};
                    BOOST_THROW_EXCEPTION(x);
                } else if (config->mode == "StdException") {
                    throw std::exception{};
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Bad Mode " << config->mode;
                }
            }
        }
    }
};

// initialize static members of Fails
std::multiset<int> Fails::phaseCalls = {};
std::mutex Fails::mutex = {};


DefaultDriver::ProgramOptions create(const std::string& yaml) {
    DefaultDriver::ProgramOptions opts;

    opts.otherProducers.emplace_back(onActorContext([&](auto& context, auto& vec) {
        for (auto i = 0; i < context.template get<int, false>("Threads").value_or(1); ++i) {
            vec.push_back(std::make_unique<Fails>(context));
        }
    }));

    opts.metricsFormat = "csv";
    opts.metricsOutputFileName = "metrics.csv";
    opts.mongoUri = "mongodb://localhost:27017";
    opts.workloadSourceType = DefaultDriver::ProgramOptions::YamlSource::kString;
    opts.workloadSource = yaml;

    return opts;
}

std::pair<DefaultDriver::OutcomeCode,DefaultDriver::ProgramOptions> outcome(const std::string& yaml) {
    DefaultDriver driver;
    auto opts = create(yaml);
    return {driver.run(opts), opts};
}

}  // namespace


TEST_CASE("Various Actor Behaviors") {

    Fails::phaseCalls.clear();
    remove("metrics.csv");

    SECTION("Normal Execution") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
          - Mode: None
            Repeat: 1
        )");
        REQUIRE((code == DefaultDriver::OutcomeCode::kSuccess));
        REQUIRE(Fails::phaseCalls == std::multiset<int>{0});
        REQUIRE(hasMetrics(opts));
    }

    SECTION("Normal Execution: Two Repeat") {
        auto [code, opts] = outcome(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Type: Fails
          Threads: 1
          Phases:
          - Mode: None
            Repeat: 2
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kSuccess);
        REQUIRE(Fails::phaseCalls == std::multiset<int>{0, 0});
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
        REQUIRE(Fails::phaseCalls == std::multiset<int>{0});
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
        REQUIRE(Fails::phaseCalls == std::multiset<int>{0});
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
              Mode: None
            - Repeat: 1
              Mode: BoostException
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kBoostException);
        REQUIRE((Fails::phaseCalls == std::multiset<int>{0, 0, 1, 1} ||
                 Fails::phaseCalls == std::multiset<int>{0, 0, 1}));
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
              Mode: None
        )");
        REQUIRE(code == DefaultDriver::OutcomeCode::kBoostException);
        REQUIRE(Fails::phaseCalls == std::multiset<int>{0});
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

        REQUIRE(Fails::phaseCalls.size() > 0);
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

        REQUIRE((Fails::phaseCalls == std::multiset<int>{0, 0} ||
                 Fails::phaseCalls == std::multiset<int>{0}));
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
        REQUIRE((Fails::phaseCalls == std::multiset<int>{0, 0} ||
                 Fails::phaseCalls == std::multiset<int>{0}));
        REQUIRE(hasMetrics(opts));
    }
}
