#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include <mongocxx/instance.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/context.hpp>

#include <gennylib/actors/HelloWorld.hpp>
#include <gennylib/actors/Insert.hpp>
#include <gennylib/actors/InsertRemove.hpp>
#include <gennylib/actors/Loader.hpp>
#include <gennylib/actors/MultiCollectionQuery.hpp>
#include <gennylib/actors/MultiCollectionUpdate.hpp>
// NextActorHeaderHere

#include "DefaultDriver.hpp"


namespace {

using namespace genny;

YAML::Node loadConfig(const std::string& source,
                      genny::driver::ProgramOptions::YamlSource sourceType) {
    if (sourceType == genny::driver::ProgramOptions::YamlSource::kString) {
        return YAML::Load(source);
    }
    try {
        return YAML::LoadFile(source);
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error loading yaml from " << source << ": " << ex.what();
        throw;
    }
}

template <typename Actor>
void runActor(Actor&& actor,
              std::atomic<driver::DefaultDriver::OutcomeCode>& outcomeCode,
              Orchestrator& orchestrator) {
    try {
        actor->run();
    } catch (const boost::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "boost::exception: " << boost::diagnostic_information(x, true);
        outcomeCode = driver::DefaultDriver::OutcomeCode::kBoostException;
        orchestrator.abort();
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "std::exception: " << x.what();
        outcomeCode = driver::DefaultDriver::OutcomeCode::kStandardException;
        orchestrator.abort();
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Unknown error";
        orchestrator.abort();
        // Don't try to handle unknown errors, let us crash ungracefully
        throw;
    }
}

genny::driver::DefaultDriver::OutcomeCode doRunLogic(const genny::driver::ProgramOptions& options) {
    genny::metrics::Registry metrics;

    auto actorSetup = metrics.timer("Genny.Setup");
    auto setupTimer = actorSetup.start();

    mongocxx::instance::current();

    auto yaml = loadConfig(options.workloadSource, options.workloadSourceType);
    auto orchestrator = Orchestrator{};

    auto producers = std::vector<genny::ActorProducer>{
        &genny::actor::HelloWorld::producer,
        &genny::actor::Insert::producer,
        &genny::actor::InsertRemove::producer,
        &genny::actor::MultiCollectionUpdate::producer,
        &genny::actor::Loader::producer,
        &genny::actor::MultiCollectionQuery::producer,
        // NextActorProducerHere
    };
    // clang-format on

    // The ProgramOptions struct can add some more ActorProducers.
    // Add them at then end so they will produce Actors last.
    // This is used primarily for testing so tests can inject test-only Actors.
    producers.insert(producers.end(), options.otherProducers.begin(), options.otherProducers.end());

    auto workloadContext =
        WorkloadContext{yaml, metrics, orchestrator, options.mongoUri, producers};

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));

    setupTimer.report();

    auto activeActors = metrics.counter("Genny.ActiveActors");

    std::atomic<driver::DefaultDriver::OutcomeCode> outcomeCode =
        driver::DefaultDriver::OutcomeCode::kSuccess;

    std::mutex lock;
    std::vector<std::thread> threads;
    std::transform(cbegin(workloadContext.actors()),
                   cend(workloadContext.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) {
                       return std::thread{[&]() {
                           lock.lock();
                           activeActors.incr();
                           lock.unlock();

                           runActor(actor, outcomeCode, orchestrator);

                           lock.lock();
                           activeActors.decr();
                           lock.unlock();
                       }};
                   });

    for (auto& thread : threads)
        thread.join();

    const auto reporter = genny::metrics::Reporter{metrics};

    std::ofstream metricsOutput;
    metricsOutput.open(options.metricsOutputFileName, std::ofstream::out | std::ofstream::app);
    reporter.report(metricsOutput, options.metricsFormat);
    metricsOutput.close();

    return outcomeCode;
}

}  // namespace


genny::driver::DefaultDriver::OutcomeCode genny::driver::DefaultDriver::run(
    const genny::driver::ProgramOptions& options) const {
    try {
        // Wrap doRunLogic in another catch block in case it throws an exception of its own e.g.
        // file not found or io errors etc - exceptions not thrown by ActorProducers.
        return doRunLogic(options);
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(error) << "Caught exception " << x.what();
    }
    return genny::driver::DefaultDriver::OutcomeCode::kInternalException;
}


namespace {

/**
 * Normalize the metrics output file command-line option.
 *
 * @param str the input option value from the command-lien
 * @return the file-path that should be used to open the output stream.
 */
// There may be a more conventional way to define conversion/normalization
// functions for use with boost::program_options. The tutorial isn't the
// clearest thing. If we need to do more than 1-2, look into that further.
std::string normalizeOutputFile(const std::string& str) {
    if (str == "-") {
        return std::string("/dev/stdout");
    }
    return str;
}

}  // namespace


genny::driver::ProgramOptions::ProgramOptions(int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description description{u8"🧞‍ Allowed Options 🧞‍"};
    po::positional_options_description positional;

    // clang-format off
    description.add_options()
        ("help,h",
            "Show help message")
        ("metrics-format,m",
             po::value<std::string>()->default_value("csv"),
             "Metrics format to use")
        ("metrics-output-file,o",
            po::value<std::string>()->default_value("/dev/stdout"),
            "Save metrics data to this file. Use `-` or `/dev/stdout` for stdout.")
        ("workload-file,w",
            po::value<std::string>(),
            "Path to workload configuration yaml file. "
            "Paths are relative to the program's cwd. "
            "Can also specify as first positional argument.")
        ("mongo-uri,u",
            po::value<std::string>()->default_value("mongodb://localhost:27017"),
            "Mongo URI to use for the default connection-pool.")
    ;

    positional.add("workload-file", -1);

    auto run = po::command_line_parser(argc, argv)
        .options(description)
        .positional(positional)
        .run();
    // clang-format on

    {
        auto stream = std::ostringstream();
        stream << description;
        this->description = stream.str();
    }

    po::variables_map vm;
    po::store(run, vm);
    po::notify(vm);

    this->isHelp = vm.count("help") >= 1;
    this->metricsFormat = vm["metrics-format"].as<std::string>();
    this->metricsOutputFileName = normalizeOutputFile(vm["metrics-output-file"].as<std::string>());
    this->mongoUri = vm["mongo-uri"].as<std::string>();

    if (vm.count("workload-file") > 0) {
        this->workloadSource = vm["workload-file"].as<std::string>();
        this->workloadSourceType = YamlSource::kFile;
    } else {
        this->workloadSourceType = YamlSource::kString;
    }
}
