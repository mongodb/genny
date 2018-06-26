#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <yaml-cpp/yaml.h>

#include <boost/noncopyable.hpp>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

class WorkloadConfig : private boost::noncopyable {

public:
    WorkloadConfig(const YAML::Node& node, metrics::Registry& registry, Orchestrator& orchestrator)
        : _node{node},
          _registry{&registry},
          _orchestrator{&orchestrator},
          _actorConfigs{createActorConfigs(node, *this)} {}

    WorkloadConfig(YAML::Node&&, metrics::Registry&&, Orchestrator&&) = delete;

    void operator=(WorkloadConfig&&) = delete;
    WorkloadConfig(WorkloadConfig&&) = delete;

    Orchestrator* orchestrator() const {
        return _orchestrator;
    }
    metrics::Registry* registry() const {
        return _registry;
    }

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
    const std::vector<std::unique_ptr<ActorConfig>> _actorConfigs;

    static std::vector<std::unique_ptr<ActorConfig>> createActorConfigs(
        const YAML::Node& node, WorkloadConfig& workloadConfig);
};


class ActorConfig : private boost::noncopyable {

public:
    void operator=(ActorConfig&&) = delete;
    ActorConfig(ActorConfig&&) = delete;

    ActorConfig(const YAML::Node& node, WorkloadConfig& config)
        : _node{node}, _workloadConfig{&config} {}

    const YAML::Node get(const std::string& key) const {
        return this->_node[key];
    }

private:
    const YAML::Node _node;
    WorkloadConfig* _workloadConfig;
};


class PhasedActorFactory : private boost::noncopyable {

public:
    void operator=(PhasedActorFactory&&) = delete;
    PhasedActorFactory(PhasedActorFactory&&) = delete;

    using ActorList = std::vector<std::unique_ptr<PhasedActor>>;
    using Producer = std::function<ActorList(ActorConfig*, WorkloadConfig*)>;

    template <class... Args>
    void addProducer(Args&&... args) {
        _producers.emplace_back(std::forward<Args>(args)...);
    }

    ActorList actors(WorkloadConfig* workloadConfig) const;

private:
    std::vector<Producer> _producers;
};


}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
