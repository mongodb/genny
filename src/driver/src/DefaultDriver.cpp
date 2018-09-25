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

    mongocxx::instance instance{};

    auto actorSetupTimer = metrics.timer("actorSetup");
    auto threadCounter = metrics.counter("threadCounter");

    auto stopwatch = actorSetupTimer.start();
    auto yaml = loadConfig(options.workloadFileName);
    auto registry = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{};

    auto producers = std::vector<genny::ActorProducer>{&genny::actor::HelloWorld::producer,
                                                       &genny::actor::Insert::producer};
    auto workloadContext = WorkloadContext{yaml, registry, orchestrator, producers};

    orchestrator.addRequiredTokens(
        int(std::distance(workloadContext.actors().begin(), workloadContext.actors().end())));
    orchestrator.phasesAtLeastTo(1);  // will later come from reading the yaml!

    stopwatch.report();

    std::mutex lock;
    std::vector<std::thread> threads;
    std::transform(cbegin(workloadContext.actors()),
                   cend(workloadContext.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) {
                       return std::thread{[&]() {
                           lock.lock();
                           threadCounter.incr();
                           lock.unlock();

                           actor->run();

                           lock.lock();
                           threadCounter.decr();
                           lock.unlock();
                       }};
                   });

    for (auto& thread : threads)
        thread.join();

    const auto reporter = genny::metrics::Reporter{registry};
    reporter.report(std::cout, options.metricsFormat);

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
    this->workloadFileName = vm["workload-file"].as<std::string>();
}
