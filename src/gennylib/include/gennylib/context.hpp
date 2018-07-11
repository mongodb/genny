#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <functional>
#include <iterator>
#include <list>
#include <type_traits>

#include <boost/noncopyable.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/metrics.hpp>
#include <gennylib/Orchestrator.hpp>

/**
 * This file defines {@code WorkloadContext} and {@code ActorContext} which provide access
 * to configuration values and other workload collaborators (e.g. metrics) during the construction
 * of actors.
 */


/**
 * This is all helper/private implementation details. Ideally this section could
 * be defined below the important stuff, but we live in a cruel world.
 */
namespace genny::detail {

/**
 * The "path" to a configured value. E.g. given the structure
 *
 * <pre>
 * foo:
 *   bar:
 *     baz: [10,20,30]
 * </pre>
 *
 * The path to the 10 is "foo/bar/baz/0".
 */
class ConfigPath {

public:
    ConfigPath() = default;

    ConfigPath(ConfigPath&) = delete;
    void operator=(ConfigPath&) = delete;

    template <class T>
    void add(const T& elt) {
        _elts.push_back(elt);
    }
    auto begin() const {
        return std::begin(_elts);
    }
    auto end() const {
        return std::end(_elts);
    }

private:
    std::list<std::function<void(std::ostream&)>> _elts;

};

// Support putting ConfigPaths onto ostreams
inline std::ostream& operator<<(std::ostream& out, const ConfigPath& p) {
    for(const auto& f : p) {
        f(out);
        out << "/";
    }
    return out;
}

// Used by get() in WorkloadContext and ActorContext
//
// This is the base-case when we're out of Args... expansions in the other helper below
template <class Out, class Current>
Out get_helper(const ConfigPath& parent, const Current& curr) {
    if (!curr) {
        std::stringstream error;
        error << "Invalid key at path [" << parent << "]";
        throw InvalidConfigurationException(error.str());
    }
    try {
        return curr.template as<Out>();
    } catch (const YAML::BadConversion& conv) {
        std::stringstream error;
        // typeid(Out).name() is kinda hokey but could be useful when debugging config issues.
        error << "Bad conversion of [" << curr << "] to [" << typeid(Out).name() << "] "
              << "at path [" << parent << "]: " << conv.what();
        throw InvalidConfigurationException(error.str());
    }
}

// Used by get() in WorkloadContext and ActorContext
//
// Recursive case where we pick off first item and recurse:
//      get_helper(foo, a, b, c) // this fn
//   -> get_helper(foo[a], b, c) // this fn
//   -> get_helper(foo[a][b], c) // this fn
//   -> get_helper(foo[a][b][c]) // "base case" fn above
template <class Out, class Current, class PathFirst, class... PathRest>
Out get_helper(ConfigPath& parent, const Current& curr, PathFirst&& pathFirst, PathRest&&... rest) {
    if (curr.IsScalar()) {
        std::stringstream error;
        error << "Wanted [" << parent << pathFirst << "] but [" << parent << "] is scalar: [" << curr
              << "]";
        throw InvalidConfigurationException(error.str());
    }
    const auto& next = curr[std::forward<PathFirst>(pathFirst)];

    parent.add([&](std::ostream& out) { out << pathFirst; });

    if (!next.IsDefined()) {
        std::stringstream error;
        error << "Invalid key [" << pathFirst << "] at path [" << parent << "]. Last accessed [" << curr
              << "].";
        throw InvalidConfigurationException(error.str());
    }
    return detail::get_helper<Out>(parent, next, std::forward<PathRest>(rest)...);
}

}  // namespace genny::detail


namespace genny {

/**
 * Represents the top-level/"global" configuration and context for configuring actors.
 * Call .get() to access top-level yaml configs.
 */
class WorkloadContext {

public:
    WorkloadContext(const YAML::Node& node,
                    metrics::Registry& registry,
                    Orchestrator& orchestrator,
                    const std::vector<ActorProducer>& producers)
            : _node{node},
              _registry{&registry},
              _orchestrator{&orchestrator},
              _actorContexts{constructActorContexts()},
              _actors{constructActors(producers)} {
        // This is good enough for now. Later can add a WorkloadContextValidator concept
        // and wire in a vector of those similar to how we do with the vector of Producers.
        if (get<std::string>("SchemaVersion") != "2018-07-01") {
            throw InvalidConfigurationException("Invalid schema version");
        }
    }

    // no copy or move
    WorkloadContext(WorkloadContext&) = delete;
    void operator=(WorkloadContext&) = delete;
    WorkloadContext(WorkloadContext&&) = default;
    void operator=(WorkloadContext&&) = delete;

    template <class T = YAML::Node, class... Args>
    T get(Args&&... args) const {
        detail::ConfigPath p;
        return detail::get_helper<T>(p, _node, std::forward<Args>(args)...);
    };

    const ActorVector& actors() const {
        return _actors;
    }

private:
    friend class ActorContext;

    ActorVector constructActors(const std::vector<ActorProducer>& producers);
    std::vector<std::unique_ptr<ActorContext>> constructActorContexts();

    YAML::Node _node;
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

    template <class T = YAML::Node, class... Args>
    T get(Args&&... args) const {
        detail::ConfigPath p;
        return detail::get_helper<T>(p, _node, std::forward<Args>(args)...);
    };

    Orchestrator* orchestrator() const {
        return this->_workload->_orchestrator;
    }

    WorkloadContext& workload() const {
        return *_workload;
    }

    // just convenience forwarding methods to avoid having to do context.registry().timer(...)

    template <class... Args>
    auto timer(Args&&... args) const {
        return this->_workload->_registry->timer(std::forward<Args>(args)...);
    }

    template <class... Args>
    auto gauge(Args&&... args) const {
        return this->_workload->_registry->gauge(std::forward<Args>(args)...);
    }

    template <class... Args>
    auto counter(Args&&... args) const {
        return this->_workload->_registry->counter(std::forward<Args>(args)...);
    }

private:
    YAML::Node _node;
    WorkloadContext* _workload;

};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
