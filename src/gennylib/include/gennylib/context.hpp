#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <iterator>
#include <list>
#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <boost/noncopyable.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/metrics.hpp>

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

struct path {

    path(path&) = delete;
    void operator=(path&) = delete;
    path() = default;

    std::list<std::string> _elts;
    template <class T>
    void add(const T& elt) {
        std::ostringstream out;
        out << elt;
        _elts.push_back(out.str());
    }
    auto begin() const {
        return std::begin(_elts);
    }
    auto end() const {
        return std::end(_elts);
    }
};

inline std::ostream& operator<<(std::ostream& out, const path& p) {
    std::copy(std::cbegin(p), std::cend(p), std::ostream_iterator<std::string>(out, "/"));
    return out;
}

template <class Out, class Current>
Out get_helper(const path& parent, const Current& curr) {
    if (!curr) {
        std::stringstream error;
        error << "Invalid key at path [" << parent << "]";
        throw InvalidConfigurationException(error.str());
    }
    try {
        return curr.template as<Out>();
    } catch (const YAML::BadConversion& conv) {
        std::stringstream error;
        error << "Bad conversion of [" << curr << "] to [" << typeid(Out).name() << "] "
              << "at path [" << parent << "]: " << conv.what();
        throw InvalidConfigurationException(error.str());
    }
}

template <class Out, class Current, class PathFirst, class... PathRest>
Out get_helper(path& parent, const Current& curr, PathFirst&& pathFirst, PathRest&&... rest) {
    if (curr.IsScalar()) {
        std::stringstream error;
        error << "Wanted [" << parent << "/" << pathFirst << "] but [" << parent << "] is scalar: [" << curr
              << "]";
        throw InvalidConfigurationException(error.str());
    }
    const auto& next = curr[std::forward<PathFirst>(pathFirst)];

    parent.add(pathFirst);

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
 */
class WorkloadContext {

public:
    // no copy or move
    WorkloadContext(WorkloadContext&) = delete;
    void operator=(WorkloadContext&) = delete;
    WorkloadContext(WorkloadContext&&) = default;
    void operator=(WorkloadContext&&) = delete;

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

    const ActorVector& actors() const {
        return _actors;
    }

    template <class O = YAML::Node, class... Args>
    O get(Args&&... args) const {
        detail::path p;
        return detail::get_helper<O>(p, _node, std::forward<Args>(args)...);
    };

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

    ActorContext() = default;

    ActorContext(const YAML::Node& node, WorkloadContext& workloadContext)
        : _node{node}, _workload{&workloadContext} {}

    // no copy or move
    ActorContext(ActorContext&) = delete;
    void operator=(ActorContext&) = delete;
    ActorContext(ActorContext&&) = default;
    void operator=(ActorContext&&) = delete;

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

    Orchestrator* orchestrator() const {
        return this->_workload->_orchestrator;
    }

    template <class O = YAML::Node, class... Args>
    O get(Args&&... args) const {
        detail::path p;
        return detail::get_helper<O>(p, _node, std::forward<Args>(args)...);
    };

    WorkloadContext& workload() const {
        return *_workload;
    }

private:
    YAML::Node _node;
    WorkloadContext* _workload;
};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
