#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <thread>
#include <vector>

#include <fstream>
#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>
#include <gennylib/config.hpp>

#include <yaml-cpp/yaml.h>

#include "DefaultDriver.hpp"


namespace {

YAML::Node loadConfig(char* const* argv) {
    const char* fileName = argv[1];
    try {
        return YAML::LoadFile(fileName);
    } catch (const std::exception& ex) {
        std::cerr << "Error loading yaml from " << fileName << ": " << ex.what();
        throw ex;
    }
}

// TODO: move to static method of HelloWorld
std::vector<std::unique_ptr<genny::PhasedActor>> helloWorldProducer(
    const genny::ActorConfig& actorConfig,
    genny::ErrorBag*) {
    const auto count = actorConfig["Count"].as<int>();
    auto out = std::vector<std::unique_ptr<genny::PhasedActor>>{};
    for (int i = 0; i < count; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(actorConfig, std::to_string(i)));
    }
    return out;
}

}  // namespace


int genny::driver::DefaultDriver::run(int argc, char** argv) const {

    auto yaml = loadConfig(argv);
    auto metrics = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{};

    ErrorBag errorBag;
    genny::PhasedActorFactory factory = {yaml, metrics, orchestrator, errorBag};

    // add producers
    factory.addProducer(&helloWorldProducer);

    const auto actors = factory.actors(&errorBag);

    if (errorBag) {
        errorBag.report(std::cerr);
        throw std::logic_error("Invalid configuration or setup");
    }

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
