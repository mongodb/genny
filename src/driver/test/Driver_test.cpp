#include <string>
#include <fstream>
#include <streambuf>

#include <boost/exception/exception.hpp>
#include <boost/log/trivial.hpp>

#include <DefaultDriver.hpp>
#include <helpers.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>


using namespace genny::driver;

namespace {

std::string readFile(const std::string& fileName) {
    std::ifstream t("file.txt");
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    return str;
}

}

template <class F>
auto onActorContext(F&& callback) {
    return [&](genny::ActorContext& context) {
        genny::ActorVector vec;
        callback(context, vec);
        return vec;
    };
}

class SomeException : public boost::exception {};

struct Fails : public genny::Actor {
    struct PhaseConfig {
        std::string mode;
        PhaseConfig(genny::PhaseContext& phaseContext)
            : mode{phaseContext.get<std::string>("Mode")} {}
    };
    genny::PhaseLoop<PhaseConfig> loop;

    explicit Fails(genny::ActorContext& ctx) : loop{ctx} {}

    void run() override {
        for (auto&& [phase, config] : loop) {
            for (auto&& _ : config) {
                if (config->mode == "None") {
                    continue;
                } else if (config->mode == "BoostException") {
                    throw SomeException{};
                } else if (config->mode == "StdException") {
                    throw std::exception{};
                } else if (config->mode == "OtherThrowable") {
                    throw "SomethingElse";
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Bad Mode " << config->mode;
                }
            }
        }
    }
};


TEST_CASE("Normal Execution") {
    DefaultDriver driver;

    BOOST_LOG_TRIVIAL(info) << cwd();

    ProgramOptions opts;

    opts.otherProducers.emplace_back(onActorContext(
        [&](auto& context, auto& vec) { vec.push_back(std::make_unique<Fails>(context)); }));

    opts.metricsFormat = "csv";
    opts.metricsOutputFileName = "metrics.csv";
    opts.mongoUri = "mongodb://localhost:27017";
    opts.sourceType = ProgramOptions::YamlSource::STRING;
    opts.workloadSource = R"(
SchemaVersion: 2018-07-01

Actors:
  - Type: Fails
    Threads: 2
    Phases:
      - Repeat: 1
        Mode: BoostException
)";
    auto outcome = driver.run(opts);
    REQUIRE(outcome == 10);
}
