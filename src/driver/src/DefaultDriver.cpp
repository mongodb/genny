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

int genny::driver::DefaultDriver::run(int argc, char**argv) const {

    auto metrics = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{1};
    auto factory = genny::ActorFactory<PhasedActor>{};

    auto producer = [&](const genny::WorkloadConfig* config) {
        static int i = 0;
        return std::make_unique<genny::actor::HelloWorld>(orchestrator, metrics, std::to_string(++i));
    };

    factory.addProducer("HelloWorld", producer);

    // test we can load yaml (just smoke-testing yaml for now, this will be real soon!)
    const char* fileName = argv[1];
    if (argc >= 3 && strncmp("--debug", argv[2], 100) != 0) {
        fileName = argv[2];
        auto yamlFile = std::ifstream {fileName, std::ios_base::binary};
        std::cout << "YAML File" << fileName << ":" << std::endl << yamlFile.rdbuf();
    }

    auto yaml = YAML::LoadFile(fileName);

    std::cout << std::endl << "Using Schema Version:" << std::endl
              << yaml["SchemaVersion"].as<std::string>() << std::endl;

    auto config = genny::WorkloadConfig{yaml, metrics, orchestrator};

    std::vector<std::unique_ptr<genny::PhasedActor>> actors = factory.actors(&config);
    orchestrator.setActorCount(actors.size());
//    assert(actors.size() == nActors);

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
