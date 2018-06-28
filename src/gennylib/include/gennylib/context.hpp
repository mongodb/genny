#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <boost/noncopyable.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ErrorBag.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

class ActorContext;

/**
 * Represents the top-level/"global" configuration and context for configuring actors.
 */
class WorkloadContext {

public:
    using ActorVector = typename std::vector<std::unique_ptr<Actor>>;
    using Producer = typename std::function<ActorVector(ActorContext&)>;

    // no copy or move
    WorkloadContext(WorkloadContext&) = delete;
    void operator=(WorkloadContext&) = delete;
    WorkloadContext(WorkloadContext&&) = default;
    void operator=(WorkloadContext&&) = delete;

    WorkloadContext(const YAML::Node& node,
                    metrics::Registry& registry,
                    Orchestrator& orchestrator,
                    const std::vector<Producer>& producers)
    : _node{node},
      _errors{},
      _registry{&registry},
      _orchestrator{&orchestrator},
      _actorContexts{constructActorContexts()},
      _actors{constructActors(producers)} {}

    template<class...Args>
    YAML::Node operator[](Args&&...args) const {
        return _node.operator[](std::forward<Args>(args)...);
    }

    const ErrorBag& errors() const {
        return _errors;
    }

    const ActorVector& actors() const {
        return _actors;
    }

private:
    friend class ActorContext;

    ActorVector constructActors(const std::vector<Producer>& producers);
    std::vector<std::unique_ptr<ActorContext>> constructActorContexts();

    YAML::Node _node;
    ErrorBag _errors;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    std::vector<std::unique_ptr<ActorContext>> _actorContexts;
    ActorVector _actors;
};

/**
 * Represents each {@code Actor:} block within a WorkloadConfig.
 */
class ActorContext {

public:
    ActorContext(const YAML::Node& node, WorkloadContext& workloadContext)
            : _node{node}, _workload{&workloadContext} {}

    // no copy or move
    ActorContext(ActorContext&) = delete;
    void operator=(ActorContext&) = delete;
    ActorContext(ActorContext&&) = default;
    void operator=(ActorContext&&) = delete;

    // just convenience forwarding methods to avoid having to do context.registry().timer(...)

    template<class...Args>
    auto timer(Args&&...args) const {
        return this->_workload->_registry->timer(std::forward<Args>(args)...);
    }

    template<class...Args>
    auto gauge(Args&&...args) const {
        return this->_workload->_registry->gauge(std::forward<Args>(args)...);
    }

    template<class...Args>
    auto counter(Args&&...args) const {
        return this->_workload->_registry->counter(std::forward<Args>(args)...);
    }

    Orchestrator* orchestrator() const {
        return this->_workload->_orchestrator;
    }

    // Act like the wrapped YAML::Node, so ctx["foo"] gives you node["foo"]
    template <class... Args>
    YAML::Node operator[](Args&&... args) const {
        return _node.operator[](std::forward<Args>(args)...);
    }

    // lets you do
    //   ctx.require(ctx["foo"], "bar", 3); // assert ctx["foo"]["bar"] == 3
    template <class Arg0,
              class... Args,
              typename = typename std::enable_if<std::is_base_of<YAML::Node, Arg0>::value>::type>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workload->_errors.require(std::forward<Arg0>(arg0),
                                                 std::forward<Args>(args)...);
    }

    // lets you do
    //   ctx.require("foo", 3); // assert ctx["foo"] == 3
    template <class Arg0,
              class... Args,
              typename = typename std::enable_if<!std::is_base_of<YAML::Node, Arg0>::value>::type,
              typename = void>
    void require(Arg0&& arg0, Args&&... args) {
        this->_workload->_errors.require(
            *this, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

private:
    YAML::Node _node;
    WorkloadContext* _workload;
};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
