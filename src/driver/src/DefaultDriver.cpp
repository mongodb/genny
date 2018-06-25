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
    auto orchestrator = Orchestrator{nActors};

    std::vector<std::unique_ptr<genny::PhasedActor>> actors;
    actors.push_back(std::make_unique<genny::actor::HelloWorld>(orchestrator, metrics, "one"));
    actors.push_back(std::make_unique<genny::actor::HelloWorld>(orchestrator, metrics, "two"));

    assert(actors.size() == nActors);

    std::vector<std::thread> threads;
    std::transform(
        cbegin(actors), cend(actors), std::back_inserter(threads), [](const auto& actor) {
            return std::thread{&genny::PhasedActor::run, actor.get()};
        });

    for (auto& thread : threads)
        thread.join();

    const auto reporter = genny::metrics::Reporter{metrics};
    reporter.report(std::cout);

    return 0;
}
