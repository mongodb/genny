#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <vector>

#include <boost/log/trivial.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>
#include <gennylib/context.hpp>


#include "DefaultDriver.hpp"


namespace {

YAML::Node loadConfig(const char* fileName) {
    try {
        return YAML::LoadFile(fileName);
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error loading yaml from " << fileName << ": " << ex.what();
        throw ex;
    }
}

}  // namespace


int genny::driver::DefaultDriver::run(int, char** argv) const {

    auto yaml = loadConfig(argv[1]);
    auto metrics = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{};

    auto producers = std::vector<genny::WorkloadContext::Producer>{&genny::actor::HelloWorld::producer};
    auto results = WorkloadContext{yaml, metrics, orchestrator, producers};

    orchestrator.setActorCount(static_cast<unsigned int>(results.actors().size()));

    std::vector<std::thread> threads;
    std::transform(cbegin(results.actors()),
                   cend(results.actors()),
                   std::back_inserter(threads),
                   [](const auto& actor) {
                       return std::thread{&genny::Actor::run, actor.get()};
                   });


    for (auto& thread : threads)
        thread.join();

    const auto reporter = genny::metrics::Reporter{metrics};
    reporter.report(std::cout);

    return 0;
}
