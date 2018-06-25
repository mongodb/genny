#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <vector>

#include <gennylib/ActorFactory.hpp>
#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>
#include <fstream>

#include "DefaultDriver.hpp"

namespace {

YAML::Node loadConfig(char *const *argv) {
    const char* fileName = argv[1];
    auto yaml = YAML::LoadFile(fileName);
    return yaml;
}

std::unique_ptr<genny::actor::HelloWorld> helloWorldProducer(genny::WorkloadConfig* config) {
    static int i = 0;
    return std::make_unique<genny::actor::HelloWorld>(*config->orchestrator(), *config->registry(), std::to_string(++i));
}

}  // namespace

int genny::driver::DefaultDriver::run(int argc, char**argv) const {

    auto metrics = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{};
    auto factory = genny::ActorFactory<PhasedActor>{};
    auto yaml = loadConfig(argv);

    auto config = genny::WorkloadConfig{
        yaml,
        metrics,
        orchestrator
    };

    // add producers
    factory.addProducer("HelloWorld", &helloWorldProducer);

    const auto actors = factory.actors(&config);

    orchestrator.setActorCount(static_cast<unsigned int>(actors.size()));

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
