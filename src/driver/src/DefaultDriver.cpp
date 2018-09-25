#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <vector>

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include <mongocxx/instance.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>
#include <gennylib/actors/Insert.hpp>
#include <gennylib/context.hpp>


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

    auto producers = std::vector<genny::ActorProducer>{&genny::actor::HelloWorld::producer,
                                                       &genny::actor::Insert::producer};
    auto workloadContext = WorkloadContext{yaml, metrics, orchestrator, producers};

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));
    orchestrator.phasesAtLeastTo(1);  // will later come from reading the yaml!

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

genny::driver::ProgramOptions::ProgramOptions(int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description description{u8"🧞‍ Allowed Options 🧞‍"};
    po::positional_options_description positional;

    // clang-format off
    description.add_options()
        ("help",
            "show help message")
        ("metrics-format",
             po::value<std::string>()->default_value("csv"),
             "metrics format to use")
        ("metrics-output-file",
            po::value<std::string>()->default_value("/dev/stdout"),
            "save metrics data to this file")
        ("workload-file",
            po::value<std::string>(),
            "path to workload configuration yaml file. "
            "Can also specify as first positional argument.")
    ;

    positional.add("workload-file", -1);

    auto run = po::command_line_parser(argc, argv)
        .options(description)
        .positional(positional)
        .run();
    // clang-format on

    po::variables_map vm;
    po::store(run, vm);
    po::notify(vm);

    if (vm.count("help") || !vm.count("workload-file")) {
        std::cout << description << std::endl;
        throw std::logic_error("Help");
    }

    this->metricsFormat = vm["metrics-format"].as<std::string>();
    this->metricsOutputFileName = vm["metrics-output-file"].as<std::string>();
    this->workloadFileName = vm["workload-file"].as<std::string>();
}
