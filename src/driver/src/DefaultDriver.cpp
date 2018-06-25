#include <cassert>
#include <thread>
#include <vector>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/ActorConfig.hpp>
#include <gennylib/ActorFactory.hpp>
#include <gennylib/actors/HelloWorld.hpp>

#include "DefaultDriver.hpp"

int genny::driver::DefaultDriver::run(int, char**) const {

    auto metrics = genny::metrics::Registry {};
    auto orchestrator = Orchestrator{0};

    const auto yaml = YAML::Load("SchemaVersion: 2018-06-22");

    auto config = genny::ActorConfig{yaml, metrics, orchestrator};

    genny::ActorFactory<genny::PhasedActor> factory;

    // TODO: map string in config file to factory function
    // Can p be loaded from dlopen?
    auto p = [](ActorConfig* actorConfig) {
        std::vector<std::unique_ptr<genny::PhasedActor>> out;
        out.push_back(std::make_unique<genny::actor::HelloWorld>(actorConfig->orchestrator(), actorConfig->registry(), "one"));
        out.push_back(std::make_unique<genny::actor::HelloWorld>(actorConfig->orchestrator(), actorConfig->registry(), "two"));
        return out;
    };
    factory.hook(p);

    const auto actors = factory.actors(&config);
    const unsigned long nActors = actors.size();
    orchestrator.setActors(nActors);

    std::vector<std::thread> threads;
    for (auto&& actor : actors)
        threads.emplace_back(&genny::PhasedActor::run, &*actor);

    for (auto& thread : threads)
        thread.join();

    auto reporter = genny::metrics::Reporter{metrics};
    reporter.report(std::cout);

    return 0;
}
