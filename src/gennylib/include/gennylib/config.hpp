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

/**
 * Represents the top-level/"global" configuration and context for configuring actors.
 */
class WorkloadConfig : private boost::noncopyable {

public:
    /**
     * @return return a {@code ActorConfig} for each of hhe {@code Actors} structures.
     *         This value is created when the WorkloadConfig is constructed.
     */
    const std::vector<std::unique_ptr<class ActorConfig>>& actorConfigs() const {
        return this->_actorConfigs;
    }

private:
    friend class WorkloadContext;
    friend class ActorConfig;

    WorkloadConfig(const YAML::Node& node, metrics::Registry& registry, Orchestrator& orchestrator)
        : _node{node},
          _errorBag{},
          _registry{&registry},
          _orchestrator{&orchestrator},
          _actorConfigs{createActorConfigs()} {
        validateWorkloadConfig();
    }

    WorkloadConfig(WorkloadConfig&& other) noexcept
    : _node{other._node},
      _errorBag{std::move(other._errorBag)},
      _registry{other._registry},
      _orchestrator{other._orchestrator},
      _actorConfigs{std::move(other._actorConfigs)} {};

    const YAML::Node _node;
    ErrorBag _errorBag;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    std::vector<std::unique_ptr<ActorConfig>> _actorConfigs;

    std::vector<std::unique_ptr<ActorConfig>> createActorConfigs();
    void validateWorkloadConfig();
};

/**
 * Represents each {@code Actor:} block within a WorkloadConfig.
 */
class ActorConfig : private boost::noncopyable {

public:
    void operator=(ActorConfig&&) = delete;
    ActorConfig(ActorConfig&&) = delete;

    template<class...Args>
    auto timer(Args&&...args) const {
        return this->_workloadConfig->_registry->timer(std::forward<Args>(args)...);
    }

    template<class...Args>
    auto gauge(Args&&...args) const {
        return this->_workloadConfig->_registry->gauge(std::forward<Args>(args)...);
    }

    template<class...Args>
    auto counter(Args&&...args) const {
        return this->_workloadConfig->_registry->counter(std::forward<Args>(args)...);
    }

    Orchestrator* orchestrator() const {
        return this->_workloadConfig->_orchestrator;
    }

    // Act like the wrapped YAML::Node, so actorConfig["foo"] gives you node["foo"]
    template <class... Args>
    YAML::Node operator[](Args&&... args) const {
        return _node.operator[](std::forward<Args>(args)...);
    }

    // lets you do
    //   actorConfig.require(actorConfig["foo"], "bar", 3); // assert config["foo"]["bar"] == 3
    template <class Arg0,
              class... Args,
              // enable this version if Arg0 is a YAML::Node
              typename = typename std::enable_if<std::is_base_of<YAML::Node, Arg0>::value>::type>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workloadConfig->_errorBag.require(std::forward<Arg0>(arg0),
                                                 std::forward<Args>(args)...);
    }

    // lets you do
    //   actorConfig.require("foo", 3); // assert config["foo"] == 3
    template <class Arg0,
              class... Args,
              // enable this version if Arg0 is *not* a YAML::Node (it's something intended for this->operator[])
              typename = typename std::enable_if<!std::is_base_of<YAML::Node, Arg0>::value>::type,
              typename = void>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workloadConfig->_errorBag.require(
            *this, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

private:
    friend class WorkloadConfig;

    ActorConfig(const YAML::Node& node, WorkloadConfig& config)
        : _node{node}, _workloadConfig{&config} {}

    const YAML::Node _node;
    WorkloadConfig* const _workloadConfig;
};

class WorkloadContext {

public:
    using ActorVector = typename std::vector<std::unique_ptr<Actor>>;

    WorkloadContext(const YAML::Node& root,
                    genny::metrics::Registry& registry,
                    genny::Orchestrator& orchestrator)
    : _workloadConfig{root, registry, orchestrator} {}

    ErrorBag& errors() {
        return _workloadConfig._errorBag;
    }
    const ActorVector& actors() {
        return _actors;
    }

private:
    friend class ActorContextFactory;
    ActorVector _actors;
    WorkloadConfig _workloadConfig;
};


class ActorContextFactory : private boost::noncopyable {

public:
    explicit ActorContextFactory() = default;

    void operator=(ActorContextFactory&&) = delete;
    ActorContextFactory(ActorContextFactory&&) = delete;

    using Producer = std::function<typename WorkloadContext::ActorVector(ActorConfig&)>;

    template <class... Args>
    void addProducer(Args&&... args) {
        _producers.emplace_back(std::forward<Args>(args)...);
    }

    WorkloadContext build(const YAML::Node& root,
                       genny::metrics::Registry& registry,
                       genny::Orchestrator& orchestrator) const;

private:
    std::vector<Producer> _producers;
};


}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
