#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <functional>
#include <iterator>
#include <type_traits>
#include <vector>

#include <boost/noncopyable.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/metrics.hpp>

/**
 * This file defines {@code WorkloadContext} and {@code ActorContext} which provide access
 * to configuration values and other workload collaborators (e.g. metrics) during the construction
 * of actors.
 *
 * Please see the documentation below on WorkloadContext and ActorContext.
 */


/*
 * This is all helper/private implementation details. Ideally this section could
 * be defined _below_ the important stuff, but we live in a cruel world.
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
    std::vector<std::function<void(std::ostream&)>> _elts;
};

// Support putting ConfigPaths onto ostreams
inline std::ostream& operator<<(std::ostream& out, const ConfigPath& p) {
    for (const auto& f : p) {
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
        error << "Wanted [" << parent << pathFirst << "] but [" << parent << "] is scalar: ["
              << curr << "]";
        throw InvalidConfigurationException(error.str());
    }
    const auto& next = curr[std::forward<PathFirst>(pathFirst)];

    parent.add([&](std::ostream& out) { out << pathFirst; });

    if (!next.IsDefined()) {
        std::stringstream error;
        error << "Invalid key [" << pathFirst << "] at path [" << parent << "]. Last accessed ["
              << curr << "].";
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
    /**
     * @param producers
     *  producers are called eagerly at construction-time.
     */
    WorkloadContext(YAML::Node node,
                    metrics::Registry& registry,
                    Orchestrator& orchestrator,
                    const std::vector<ActorProducer>& producers)
        : _node{std::move(node)}, _registry{&registry}, _orchestrator{&orchestrator} {
        // This is good enough for now. Later can add a WorkloadContextValidator concept
        // and wire in a vector of those similar to how we do with the vector of Producers.
        if (get_static<std::string>(_node, "SchemaVersion") != "2018-07-01") {
            throw InvalidConfigurationException("Invalid schema version");
        }
        _actorContexts = constructActorContexts(_node, this);
        _actors = constructActors(producers, _actorContexts);
    }

    // no copy or move
    WorkloadContext(WorkloadContext&) = delete;
    void operator=(WorkloadContext&) = delete;
    WorkloadContext(WorkloadContext&&) = default;
    void operator=(WorkloadContext&&) = delete;

    /**
     * Retrieve configuration values from the top-level workload configuration.
     * Returns root[arg1][arg2]...[argN].
     *
     * This is somewhat expensive and should only be called during actor/workload setup.
     *
     * Typical usage:
     *
     * <pre>
     *     class MyActor ... {
     *       string name;
     *       MyActor(ActorContext& ctx)
     *       : name{ctx.get<string>("Name")} {}
     *     }
     * </pre>
     *
     * Given this YAML:
     *
     * <pre>
     *     SchemaVersion: 2018-07-01
     *     Actors:
     *     - Name: Foo
     *       Count: 100
     *     - Name: Bar
     * </pre>
     *
     * Then traverse as with the following:
     *
     * <pre>
     *     auto schema = cx.get<std::string>("SchemaVersion");
     *     auto actors = cx.get("Actors"); // actors is a YAML::Node
     *     auto name0  = cx.get<std::string>("Actors", 0, "Name");
     *     auto count0 = cx.get<int>("Actors", 0, "Count");
     *     auto name1  = cx.get<std::string>("Actors", 1, "Name");
     * </pre>
     */
    template <class T = YAML::Node, class... Args>
    static T get_static(const YAML::Node& node, Args&&... args) {
        detail::ConfigPath p;
        return detail::get_helper<T>(p, node, std::forward<Args>(args)...);
    };

    template <typename T = YAML::Node, class... Args>
    T get(Args&&... args) {
        return WorkloadContext::get_static<T>(_node, std::forward<Args>(args)...);
    };

    /**
     * @return all the actors produced. This should only be called by workload drivers.
     */
    constexpr const ActorVector& actors() const {
        return _actors;
    }

private:
    friend class ActorContext;

    // helper methods used during construction
    static ActorVector constructActors(const std::vector<ActorProducer>& producers,
                                       const std::vector<std::unique_ptr<ActorContext>>&);
    static std::vector<std::unique_ptr<ActorContext>> constructActorContexts(const YAML::Node&,
                                                                             WorkloadContext*);
    YAML::Node _node;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    // we own the child ActorContexts
    std::vector<std::unique_ptr<ActorContext>> _actorContexts;
    ActorVector _actors;
};


/**
 * Represents each {@code Actor:} block within a WorkloadConfig.
 */
class ActorContext final {

public:
    ActorContext(const YAML::Node& node, WorkloadContext& workloadContext)
        : _node{node}, _workload{&workloadContext} {}

    // no copy or move
    ActorContext(ActorContext&) = delete;
    void operator=(ActorContext&) = delete;
    ActorContext(ActorContext&&) = default;
    void operator=(ActorContext&&) = delete;

    /**
     * Retrieve configuration values from a particular 'Actor:' block in the workload configuration.
     * Returns actor[arg1][arg2]...[argN].
     *
     * This is somewhat expensive and should only be called during actor/workload setup.
     *
     * Typical usage:
     *
     * <pre>
     *     class MyActor ... {
     *       string name;
     *       MyActor(ActorContext& ctx)
     *       : name{ctx.get<string>("Name")} {}
     *     }
     * </pre>
     *
     * Given this YAML:
     *
     * <pre>
     *     SchemaVersion: 2018-07-01
     *     Actors:
     *     - Name: Foo
     *     - Name: Bar
     * </pre>
     *
     * There will be two ActorConfigs, one for {Name:Foo} and another for {Name:Bar}.
     *
     * <pre>
     *     auto name = cx.get<std::string>("Name");
     * </pre>
     *
     * If you need top-level configuration values (e.g. SchemaVersion in the above example),
     * then you can access {@code actorContext.workload()}.
     */
    template <class T = YAML::Node, class... Args>
    T get(Args&&... args) const {
        detail::ConfigPath p;
        return detail::get_helper<T>(p, _node, std::forward<Args>(args)...);
    };

    constexpr Orchestrator* orchestrator() const {
        return this->_workload->_orchestrator;
    }

    constexpr WorkloadContext& workload() const {
        return *_workload;
    }

    // just convenience forwarding methods to avoid having to do context.registry().timer(...)

    /**
     * Convenience method for creating a {@code metrics::Timer}.
     */
    template <class... Args>
    constexpr auto timer(Args&&... args) const {
        return this->_workload->_registry->timer(std::forward<Args>(args)...);
    }

    /**
     * Convenience method for creating a {@code metrics::Gauge}.
     */
    template <class... Args>
    constexpr auto gauge(Args&&... args) const {
        return this->_workload->_registry->gauge(std::forward<Args>(args)...);
    }

    /**
     * Convenience method for creating a {@code metrics::Counter}.
     */
    template <class... Args>
    constexpr auto counter(Args&&... args) const {
        return this->_workload->_registry->counter(std::forward<Args>(args)...);
    }

private:
    YAML::Node _node;
    WorkloadContext* _workload;
};

// using ActorContext= WorkloadContext::ActorContext;

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
