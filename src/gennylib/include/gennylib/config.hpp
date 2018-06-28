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
class WorkloadContext : private boost::noncopyable {

public:
    using ActorVector = typename std::vector<std::unique_ptr<Actor>>;

    /**
     * @return return a {@code ActorConfig} for each of hhe {@code Actors} structures.
     *         This value is created when the WorkloadConfig is constructed.
     */
    const std::vector<std::unique_ptr<class ActorContext>>& actorContexts() const {
        return this->_actorContexts;
    }

    ErrorBag& errors() {
        return _errorBag;
    }

    const ActorVector& actors() const {
        return _actors;
    }

private:
    friend class ActorContext;
    friend class WorkloadContextFactory;

    WorkloadContext(const YAML::Node& node, metrics::Registry& registry, Orchestrator& orchestrator)
        : _node{node},
          _errorBag{},
          _registry{&registry},
          _orchestrator{&orchestrator},
          _actorContexts{createActorConfigs()} {
        validateWorkloadConfig();
    }

    WorkloadContext(WorkloadContext&& other) noexcept
    : _node{other._node},
      _errorBag{std::move(other._errorBag)},
      _registry{other._registry},
      _orchestrator{other._orchestrator},
      _actorContexts{std::move(other._actorContexts)} {};

    std::vector<std::unique_ptr<ActorContext>> createActorConfigs();

    void validateWorkloadConfig();

    YAML::Node _node;
    ErrorBag _errorBag;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    std::vector<std::unique_ptr<ActorContext>> _actorContexts;
    ActorVector _actors;

};


/**
 * Represents each {@code Actor:} block within a WorkloadConfig.
 */
class ActorContext : private boost::noncopyable {

public:
    void operator=(ActorContext&&) = delete;
    ActorContext(ActorContext&&) = delete;

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
              typename = typename std::enable_if<std::is_base_of<YAML::Node, Arg0>::value>::type>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workloadConfig->_errorBag.require(std::forward<Arg0>(arg0),
                                                 std::forward<Args>(args)...);
    }

    // lets you do
    //   actorConfig.require("foo", 3); // assert config["foo"] == 3
    template <class Arg0,
              class... Args,
              typename = typename std::enable_if<!std::is_base_of<YAML::Node, Arg0>::value>::type,
              typename = void>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workloadConfig->_errorBag.require(
            *this, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

private:
    friend class WorkloadContext;

    ActorContext(const YAML::Node& node, WorkloadContext& config)
        : _node{node}, _workloadConfig{&config} {}

    YAML::Node _node;
    WorkloadContext* const _workloadConfig;

};


class WorkloadContextFactory : private boost::noncopyable {

public:
    using Producer = std::function<typename WorkloadContext::ActorVector(ActorContext&)>;

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
