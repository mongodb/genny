#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include <mongocxx/instance.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/context.hpp>

#include <gennylib/actors/HelloWorld.hpp>
#include <gennylib/actors/Insert.hpp>
#include <gennylib/actors/InsertRemove.hpp>
#include <gennylib/actors/MultiCollectionUpdate.hpp>
#include <gennylib/actors/Loader.hpp>
#include <gennylib/actors/MultiCollectionQuery.hpp>
// NextActorHeaderHere

#include "DefaultDriver.hpp"


namespace {

YAML::Node loadConfig(const std::string& fileName) {
    try {
        return YAML::LoadFile(fileName);
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error loading yaml from " << fileName << ": " << ex.what();
        throw;
    }
}

}  // namespace


int genny::driver::DefaultDriver::run(const genny::driver::ProgramOptions& options) const {

    genny::metrics::Registry metrics;

    auto actorSetup = metrics.timer("Genny.Setup");
    auto setupTimer = actorSetup.start();

    mongocxx::instance instance{};

    auto yaml = loadConfig(options.workloadFileName);
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

    auto workloadContext =
        WorkloadContext{yaml, metrics, orchestrator, options.mongoUri, producers};

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));

    setupTimer.report();

    auto activeActors = metrics.counter("Genny.ActiveActors");

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

                           actor->run();

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

    return 0;
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

    if (vm.count("workload-file") > 0)
        this->workloadFileName = vm["workload-file"].as<std::string>();
}
