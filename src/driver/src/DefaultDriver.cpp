#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <vector>

#include <boost/log/trivial.hpp>

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


int genny::driver::DefaultDriver::run(int argc, char** argv) const {

    if (argc < 2) {
        BOOST_LOG_TRIVIAL(fatal) << "Usage: " << argv[0] << " WORKLOAD_FILE.yml";
        return EXIT_FAILURE;
    }

    genny::metrics::Registry metrics;

    auto actorSetup = metrics.timer("Genny.Setup");
    auto setupTimer = actorSetup.start();

    mongocxx::instance instance{};

    auto yaml = loadConfig(argv[1]);
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
    reporter.report(std::cout);

    return 0;
}
