#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <yaml-cpp/yaml.h>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/metrics.hpp>
#include <gennylib/PhasedActor.hpp>

namespace genny {

class WorkloadConfig {

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

    const std::vector<std::unique_ptr<class ActorConfig>>& actorConfigs() const {
        return this->_actorConfigs;
    }

private:
    const YAML::Node _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    // computed based on _node
    const std::vector<std::unique_ptr<ActorConfig>> _actorConfigs;

    static std::vector<std::unique_ptr<ActorConfig>> createActorConfigs(const YAML::Node& node, WorkloadConfig& workloadConfig) {
        auto out = std::vector<std::unique_ptr<ActorConfig>> {};
        for(const auto& actor : node["Actors"]) {
            out.push_back(std::make_unique<ActorConfig>(actor, workloadConfig));
        }
        return out;
    }

};



class ActorConfig {

public:
    ActorConfig(const YAML::Node& node, WorkloadConfig& config)
            : _node{node}, _workloadConfig{&config} {}

    const YAML::Node get(const std::string& key) const {
        return this->_node[key];
    }

private:
    const YAML::Node _node;
    WorkloadConfig* _workloadConfig;

};


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

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
