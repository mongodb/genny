#include <cassert>
#include <thread>
#include <vector>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>

#include "DefaultDriver.hpp"

int genny::driver::DefaultDriver::run(int, char**) const {
    const int nActors = 2;

    genny::metrics::Registry metrics;
    auto orchestrator = std::make_unique<Orchestrator>(nActors);

    std::vector<std::unique_ptr<genny::PhasedActor>> actors;
    actors.push_back(std::make_unique<genny::actor::HelloWorld>(*orchestrator, metrics, "one"));
    actors.push_back(std::make_unique<genny::actor::HelloWorld>(*orchestrator, metrics, "two"));

    assert(actors.size() == nActors);

    std::vector<std::thread> threads;
    for (auto&& actor : actors)
        threads.emplace_back(&genny::PhasedActor::run, &*actor);

    for (auto& thread : threads)
        thread.join();

    auto reporter = genny::metrics::Reporter{metrics};
    reporter.report(std::cout);

    return 0;
}
