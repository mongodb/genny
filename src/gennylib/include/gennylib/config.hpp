#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <boost/noncopyable.hpp>

#include <gennylib/ErrorBag.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

class PhasedActor;

/**
 * Represents the top-level/"global" configuration. This configuration
 * typically comes from the user-provided workload config yaml.
 */
class WorkloadConfig : private boost::noncopyable {

public:
    void operator=(WorkloadConfig&&) = delete;
    WorkloadConfig(WorkloadConfig&&) = delete;

    /**
     * @return return a {@code ActorConfig} for each of hhe {@code Actors} structures.
     *         This value is created when the WorkloadConfig is constructed.
     */
    const std::vector<std::unique_ptr<class ActorConfig>>& actorConfigs() const {
        return this->_actorConfigs;
    }

private:
    friend class PhasedActorFactory;
    friend class ActorConfig;

    WorkloadConfig(const YAML::Node& node, metrics::Registry& registry, Orchestrator& orchestrator)
        : _node{node},
          _registry{&registry},
          _orchestrator{&orchestrator},
          _actorConfigs{createActorConfigs(node, *this)},
          _errorBag{} {
        validateWorkloadConfig();
    }

    const YAML::Node _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    const std::vector<std::unique_ptr<ActorConfig>> _actorConfigs;
    ErrorBag _errorBag;

    static std::vector<std::unique_ptr<ActorConfig>> createActorConfigs(
        const YAML::Node& node, WorkloadConfig& workloadConfig);

    void validateWorkloadConfig();
};


class ActorConfig : private boost::noncopyable {

public:
    void operator=(ActorConfig&&) = delete;
    ActorConfig(ActorConfig&&) = delete;

    template <class... Args>
    YAML::Node operator[](Args&&... args) const {
        return _node.operator[](std::forward<Args>(args)...);
    }

    template <class Arg0,
              class... Args,
              typename = typename std::enable_if<std::is_base_of<YAML::Node, Arg0>::value>::type>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workloadConfig->_errorBag.require(std::forward<Arg0>(arg0),
                                                 std::forward<Args>(args)...);
    }

    template <class Arg0,
              class... Args,
              typename = typename std::enable_if<!std::is_base_of<YAML::Node, Arg0>::value>::type,
              typename = void>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workloadConfig->_errorBag.require(
            *this, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

    metrics::Registry* registry() const {
        return this->_workloadConfig->_registry;
    }

    Orchestrator* orchestrator() const {
        return this->_workloadConfig->_orchestrator;
    }

private:
    friend class WorkloadConfig;

    ActorConfig(const YAML::Node& node, WorkloadConfig& config)
        : _node{node}, _workloadConfig{&config} {}

    const YAML::Node _node;
    WorkloadConfig* const _workloadConfig;
};


class PhasedActorFactory : private boost::noncopyable {

public:
    PhasedActorFactory(const YAML::Node& root,
                       genny::metrics::Registry& registry,
                       genny::Orchestrator& orchestrator);

    void operator=(PhasedActorFactory&&) = delete;
    PhasedActorFactory(PhasedActorFactory&&) = delete;

    using ActorVector = std::vector<std::unique_ptr<PhasedActor>>;
    using Producer = std::function<ActorVector(ActorConfig&)>;

    template <class... Args>
    void addProducer(Args&&... args) {
        _producers.emplace_back(std::forward<Args>(args)...);
    }

    struct Results {
        ActorVector actors;
        const ErrorBag& errorBag;
    };

    Results actors() const;

private:
    std::vector<Producer> _producers;
    const WorkloadConfig _workloadConfig;
};


}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
