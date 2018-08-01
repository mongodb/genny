#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <vector>

#include <boost/noncopyable.hpp>

#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/metrics.hpp>

/**
 * This file defines `WorkloadContext` and `ActorContext` which provide access
 * to configuration values and other workload collaborators (e.g. metrics) during the construction
 * of actors.
 *
 * Please see the documentation below on WorkloadContext and ActorContext.
 */


/*
 * This is all helper/private implementation details. Ideally this section could
 * be defined _below_ the important stuff, but we live in a cruel world.
 */
namespace genny::V1 {

/**
 * The "path" to a configured value. E.g. given the structure
 *
 * ```yaml
 * foo:
 *   bar:
 *     baz: [10,20,30]
 * ```
 *
 * The path to the 10 is "foo/bar/baz/0".
 *
 * This is used to report meaningful exceptions in the case of mis-configuration.
 */
class ConfigPath {

public:
    ConfigPath() = default;

    ConfigPath(ConfigPath&) = delete;
    void operator=(ConfigPath&) = delete;

    template <class T>
    void add(const T& elt) {
        _elements.push_back(elt);
    }
    auto begin() const {
        return std::begin(_elements);
    }
    auto end() const {
        return std::end(_elements);
    }

private:
    /**
     * The parts of the path, so for this structure
     *
     * ```yaml
     * foo:
     *   bar: [bat, baz]
     * ```
     *
     * If this `ConfigPath` represents "baz", then `_elements`
     * will be `["foo", "bar", 1]`.
     *
     * To be "efficient" we only store a `function` that produces the
     * path component string; do this to avoid (maybe) expensive
     * string-formatting in the "happy case" where the ConfigPath is
     * never fully serialized to an exception.
     */
    std::vector<std::function<void(std::ostream&)>> _elements;
};

// Support putting ConfigPaths onto ostreams
inline std::ostream& operator<<(std::ostream& out, const ConfigPath& path) {
    for (const auto& f : path) {
        f(out);
        out << "/";
    }
    return out;
}

// Used by get() in WorkloadContext and ActorContext
//
// This is the base-case when we're out of Args... expansions in the other helper below
template <class Out,
          class Current,
          bool Required = true,
          class OutV = typename std::conditional<Required, Out, std::optional<Out>>::type>
OutV get_helper(const ConfigPath& parent, const Current& curr) {
    if (!curr) {
        if constexpr (Required) {
            std::stringstream error;
            error << "Invalid key at path [" << parent << "]";
            throw InvalidConfigurationException(error.str());
        } else {
            return std::make_optional<Out>();
        }
    }
    try {
        if constexpr (Required) {
            return curr.template as<Out>();
        } else {
            return std::make_optional<Out>(curr.template as<Out>());
        }
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
template <class Out,
          class Current,
          bool Required = true,
          class OutV = typename std::conditional<Required, Out, std::optional<Out>>::type,
          class PathFirst,
          class... PathRest>
OutV get_helper(ConfigPath& parent,
                const Current& curr,
                PathFirst&& pathFirst,
                PathRest&&... rest) {
    if (curr.IsScalar()) {
        std::stringstream error;
        error << "Wanted [" << parent << pathFirst << "] but [" << parent << "] is scalar: ["
              << curr << "]";
        throw InvalidConfigurationException(error.str());
    }
    const auto& next = curr[std::forward<PathFirst>(pathFirst)];

    parent.add([&](std::ostream& out) { out << pathFirst; });

    if (!next.IsDefined()) {
        if constexpr (Required) {
            std::stringstream error;
            error << "Invalid key [" << pathFirst << "] at path [" << parent << "]. Last accessed ["
                  << curr << "].";
            throw InvalidConfigurationException(error.str());
        } else {
            return std::make_optional<Out>();
        }
    }
    return V1::get_helper<Out, Current, Required>(parent, next, std::forward<PathRest>(rest)...);
}

}  // namespace genny::V1


namespace genny {

/**
 * Represents the top-level/"global" configuration and context for configuring actors.
 * Call `.get()` to access top-level yaml configs.
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
        : _node{std::move(node)},
          _registry{&registry},
          _orchestrator{&orchestrator},
          _clientPool{mongocxx::uri{_node["MongoUri"].as<std::string>()}} {
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
     * Returns `root[arg1][arg2]...[argN]`.
     *
     * This is somewhat expensive and should only be called during actor/workload setup.
     *
     * Typical usage:
     *
     * ```c++
     *     class MyActor ... {
     *       string name;
     *       MyActor(ActorContext& context)
     *       : name{context.get<string>("Name")} {}
     *     }
     * ```
     *
     * Given this YAML:
     *
     * ```yaml
     *     SchemaVersion: 2018-07-01
     *     Actors:
     *     - Name: Foo
     *       Count: 100
     *     - Name: Bar
     * ```
     *
     * Then traverse as with the following:
     *
     * ```c++
     *     auto schema = context.get<std::string>("SchemaVersion");
     *     auto actors = context.get("Actors"); // actors is a YAML::Node
     *     auto name0  = context.get<std::string>("Actors", 0, "Name");
     *     auto count0 = context.get<int>("Actors", 0, "Count");
     *     auto name1  = context.get<std::string>("Actors", 1, "Name");
     * ```
     * @tparam T the output type required. Will forward to YAML::Node.as<T>()
     * @tparam Required If true, will error if item not found. If false, will return an optional<T>
     * that will be empty if not found.
     */
    template <class T = YAML::Node,
              bool Required = true,
              class OutV = typename std::conditional<Required, T, std::optional<T>>::type,
              class... Args>
    static OutV get_static(const YAML::Node& node, Args&&... args) {
        V1::ConfigPath p;
        return V1::get_helper<T, YAML::Node, Required>(p, node, std::forward<Args>(args)...);
    };

    /**
     * @see get_static()
     */
    template <typename T = YAML::Node,
              bool Required = true,
              class OutV = typename std::conditional<Required, T, std::optional<T>>::type,
              class... Args>
    OutV get(Args&&... args) {
        return WorkloadContext::get_static<T, Required>(_node, std::forward<Args>(args)...);
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
    mongocxx::pool _clientPool;
    ActorVector _actors;
};


/**
 * Represents each `Actor:` block within a WorkloadConfig.
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
     * Retrieve configuration values from a particular `Actor:` block in the workload configuration.
     * `Returns actor[arg1][arg2]...[argN]`.
     *
     * This is somewhat expensive and should only be called during actor/workload setup.
     *
     * Typical usage:
     *
     * ```c++
     *     class MyActor ... {
     *       string name;
     *       MyActor(ActorContext& context)
     *       : name{context.get<string>("Name")} {}
     *     }
     * ```
     *
     * Given this YAML:
     *
     * ```yaml
     *     SchemaVersion: 2018-07-01
     *     Actors:
     *     - Name: Foo
     *     - Name: Bar
     * ```
     *
     * There will be two ActorConfigs, one for `{Name:Foo}` and another for `{Name:Bar}`.
     *
     * ```
     * auto name = cx.get<std::string>("Name");
     * ```
     * @tparam T the return-value type. Will return a T if Required (and throw if not found) else
     * will return an optional<T> (empty optional if not found).
     * @tparam Required If true, will error if item not found. If false, will return an optional<T>
     * that will be empty if not found.
     */
    template <typename T = YAML::Node,
            bool Required = true,
            class OutV = typename std::conditional<Required, T, std::optional<T>>::type,
            class... Args>
    OutV get(Args&&... args) const {
        V1::ConfigPath p;
        return V1::get_helper<T, YAML::Node, Required>(p, _node, std::forward<Args>(args)...);
    };

    /**
     * Access top-level workload configuration.
     */
    WorkloadContext& workload() {
        return *this->_workload;
    }

    // just convenience forwarding methods to avoid having to do context.registry().timer(...)

    /**
     * Convenience method for creating a metrics::Timer.
     */
    template <class... Args>
    constexpr auto timer(Args&&... args) const {
        return this->_workload->_registry->timer(std::forward<Args>(args)...);
    }

    /**
     * Convenience method for creating a metrics::Gauge.
     */
    template <class... Args>
    constexpr auto gauge(Args&&... args) const {
        return this->_workload->_registry->gauge(std::forward<Args>(args)...);
    }

    /**
     * Convenience method for creating a metrics::Counter.
     */
    template <class... Args>
    constexpr auto counter(Args&&... args) const {
        return this->_workload->_registry->counter(std::forward<Args>(args)...);
    }

    // Convenience forwarders for Orchestrator

    auto morePhases() {
        return this->_workload->_orchestrator->morePhases();
    }

    auto currentPhaseNumber() {
        return this->_workload->_orchestrator->currentPhaseNumber();
    }
    auto awaitPhaseStart() {
        return this->_workload->_orchestrator->awaitPhaseStart();
    }

    auto awaitPhaseEnd() {
        return this->_workload->_orchestrator->awaitPhaseEnd();
    }

    auto abort() {
        return this->_workload->_orchestrator->abort();
    }

    mongocxx::pool::entry client() {
        return _workload->_clientPool.acquire();
    }

private:
    YAML::Node _node;
    WorkloadContext* _workload;
};

// using ActorContext= WorkloadContext::ActorContext;

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
