#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <vector>

#include <gennylib/config.hpp>
#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>
#include <fstream>

#include <yaml-cpp/yaml.h>

#include "DefaultDriver.hpp"



namespace {

YAML::Node loadConfig(char *const *argv) {
    const char* fileName = argv[1];
    auto yaml = YAML::LoadFile(fileName);
    return yaml;
}

std::vector<std::unique_ptr<genny::PhasedActor>> helloWorldProducer(const genny::ActorConfig* const actorConfig,
                                                                    const genny::WorkloadConfig* const workloadConfig) {
    auto count = actorConfig->get("Count").as<int>();
    auto out = std::vector<std::unique_ptr<genny::PhasedActor>> {};
    for(int i=0; i<count; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(workloadConfig->orchestrator(), workloadConfig->registry(), std::to_string(i)));
    }
    return out;
}

}  // namespace


int genny::driver::DefaultDriver::run(int argc, char**argv) const {

    auto yaml = loadConfig(argv);
    auto metrics = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{};

    auto factory = genny::PhasedActorFactory{yaml, metrics, orchestrator};

    // add producers
    factory.addProducer(&helloWorldProducer);

    const auto actors = factory.actors();

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
