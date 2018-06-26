#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <vector>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>
#include <fstream>

#include <yaml-cpp/yaml.h>

#include "DefaultDriver.hpp"

namespace genny {

using namespace std;

class ActorConfig;

class WorkloadConfig {

private:
    const YAML::Node _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;

public:
    WorkloadConfig(YAML::Node node,
                   metrics::Registry& registry,
                   Orchestrator& orchestrator)
    : _node{node}, _registry{&registry}, _orchestrator{&orchestrator} {}

    Orchestrator* orchestrator() { return _orchestrator; }
    metrics::Registry* registry() { return _registry; }

    const YAML::Node get(const std::string& key) {
        return this->_node[key];
    }

    const YAML::Node operator[](const std::string& key) {
        return this->get(key);
    }

    vector<ActorConfig> actorConfigs() {
        auto out = vector<ActorConfig> {};
        const auto actors = this->get("Actors");
        for(const auto& actor : actors) {
            out.emplace_back(actor, *this);
        }
        return out;
    }
};



class ActorConfig {
private:
    const YAML::Node _node;
    WorkloadConfig _workloadConfig;

public:
    Orchestrator* orchestrator() { return _workloadConfig.orchestrator(); }
    metrics::Registry* registry() { return _workloadConfig.registry(); }

    ActorConfig(YAML::Node node, WorkloadConfig config)
    : _node{node}, _workloadConfig{config} {}

    const YAML::Node get(const std::string& key) {
        return this->_node[key];
    }

    const YAML::Node operator[](const std::string& key) {
        return this->get(key);
    }
};


class WorkloadConfig;

class ActorFactory {

public:
    using Actor = std::unique_ptr<genny::PhasedActor>;
    using ActorList = std::vector<Actor>;
    using Producer = std::function<ActorList(ActorConfig, WorkloadConfig)>;

    void addProducer(const Producer function) {
        _producers.emplace_back(std::move(function));
    }

    ActorList actors(WorkloadConfig config) {
        auto out = ActorList {};
        for(Producer& producer : _producers) {
            for(ActorConfig& actorConfig : config.actorConfigs()) {
                ActorList produced = producer(actorConfig, config);
                for (auto&& actor : produced) {
                    out.push_back(std::move(actor));
                }
            }
        }
        return out;
    }

private:
    std::vector<Producer> _producers;
};





}  // genny



namespace {

YAML::Node loadConfig(char *const *argv) {
    const char* fileName = argv[1];
    auto yaml = YAML::LoadFile(fileName);
    return yaml;
}

std::vector<std::unique_ptr<genny::PhasedActor>> helloWorldProducer(genny::ActorConfig actorConfig, const genny::WorkloadConfig&) {
    auto count = actorConfig["Count"].as<int>();
    auto out = std::vector<std::unique_ptr<genny::PhasedActor>> {};
    for(int i=0; i<count; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(actorConfig.orchestrator(), actorConfig.registry(), std::to_string(i)));
    }
    return out;
}

}  // namespace


int genny::driver::DefaultDriver::run(int argc, char**argv) const {

    auto metrics = genny::metrics::Registry{};
    auto orchestrator = Orchestrator{};
    auto factory = genny::ActorFactory{};
    auto yaml = loadConfig(argv);

    auto config = genny::WorkloadConfig{
        yaml,
        metrics,
        orchestrator
    };

    // add producers
    factory.addProducer(&helloWorldProducer);

    const auto actors = factory.actors(config);

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
