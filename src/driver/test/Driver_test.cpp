#include <helpers.hpp>
#include <DefaultDriver.hpp>
#include <boost/log/trivial.hpp>


using namespace genny::driver;


TEST_CASE("Normal Execution") {
    DefaultDriver driver;

    BOOST_LOG_TRIVIAL(info) << cwd();

    ProgramOptions opts;
    opts.metricsFormat = "csv";
    opts.mongoUri = "mongodb://localhost:27017";
    opts.sourceType = ProgramOptions::YamlSource::STRING;
    opts.workloadSource = R"(
SchemaVersion: 2018-07-01

Actors:
  - Name: Actor1
    Type: Actor1
    Threads: 2
    Phases:
      - Repeat: 1
      - Repeat: 0
  - Name: Actor2
    Type: Actor2
    Threads: 2
    Phases:
      - Repeat: 0
      - Repeat: 1
)";
    auto outcome = driver.run(opts);
}
