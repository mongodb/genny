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

class ActorConfig;
class WorkloadConfig;

class WorkloadConfig {

private:
    const YAML::Node _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    // computed based on _node
    const std::vector<std::unique_ptr<ActorConfig>> _actorConfigs;

public:
    WorkloadConfig(const YAML::Node& node,
                   metrics::Registry& registry,
                   Orchestrator& orchestrator)
    : _node{node}, _registry{&registry}, _orchestrator{&orchestrator},
      _actorConfigs{createActorConfigs(node, *this)} {}

    WorkloadConfig(YAML::Node&&,
                   metrics::Registry&&,
                   Orchestrator&&) = delete;

    Orchestrator* orchestrator() const { return _orchestrator; }
    metrics::Registry* registry() const { return _registry; }

    const YAML::Node get(const std::string& key) const {
        return this->_node[key];
    }

    const std::vector<std::unique_ptr<ActorConfig>>& actorConfigs() const {
        return this->_actorConfigs;
    }

private:
    static std::vector<std::unique_ptr<ActorConfig>> createActorConfigs(const YAML::Node& node, WorkloadConfig& workloadConfig) {
        auto out = std::vector<std::unique_ptr<ActorConfig>> {};
        for(const auto& actor : node["Actors"]) {
            out.push_back(std::make_unique<ActorConfig>(actor, workloadConfig));
        }
        return out;
    }
};



class ActorConfig {
private:
    const YAML::Node _node;
    WorkloadConfig* _workloadConfig;

public:
    ActorConfig(const YAML::Node& node, WorkloadConfig& config)
    : _node{node}, _workloadConfig{&config} {}

    const YAML::Node get(const std::string& key) const {
        return this->_node[key];
    }
};


class WorkloadConfig;

class ActorFactory {

public:
    using Actor = std::unique_ptr<genny::PhasedActor>;
    using ActorList = std::vector<Actor>;
    using Producer = std::function<ActorList(ActorConfig*, WorkloadConfig*)>;

    void addProducer(const Producer &function) {
        _producers.emplace_back(function);
    }

    ActorList actors(WorkloadConfig* const workloadConfig) const {
        auto out = ActorList {};
        for(const auto& producer : _producers) {
            for(const auto& actorConfig : workloadConfig->actorConfigs()) {
                ActorList produced = producer(actorConfig.get(), workloadConfig);
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
