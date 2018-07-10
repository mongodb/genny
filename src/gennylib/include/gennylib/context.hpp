#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <boost/noncopyable.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

class InvalidConfigurationException : public std::invalid_argument {

public:
    explicit InvalidConfigurationException(const std::string &s)
    : std::invalid_argument{s} {}

    explicit InvalidConfigurationException(const char *s) : invalid_argument(s) {}

    const char *what() const throw() {
        return logic_error::what();
    }
};


namespace detail {

template<class O, class N>
O get_helper(const std::string& path, N curr) {
    if (!curr) {
        throw InvalidConfigurationException("Invalid Key at path " + path);
    }
    try {
        return curr.template as<O>();
    }
    catch(const YAML::BadConversion& conv) {
        std::stringstream error;
        error << "Bad conversion of " << curr << " to " <<  typeid(O).name() << " "
              << "at path [" << path << "]: " << conv.what();
        throw InvalidConfigurationException(error.str());
    }
}

template<class O, class N, class Arg0, class...Args>
O get_helper(std::string path, N curr, Arg0&& arg0, Args&&...args) {
    if (curr.IsScalar()) {
        throw InvalidConfigurationException(std::string{"Wanted ["} + path + "/" + arg0 + "] but [" + path + "] is scalar.");
    }
    path += std::string{"/"} + arg0;

    auto ncurr = curr[std::forward<Arg0>(arg0)];
    if (!ncurr) {
        throw InvalidConfigurationException(std::string{"Invalid key ["} + arg0 + "] at path [" + path + "]");
    }
    return detail::get_helper<O>(path, ncurr, std::forward<Args>(args)...);
}

}  // namespace detail


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
          _registry{&registry},
          _orchestrator{&orchestrator},
          _actorContexts{constructActorContexts()},
          _actors{constructActors(producers)} {

        if (get<std::string>("SchemaVersion") != "2018-07-01") {
            throw InvalidConfigurationException("Invalid schema version");
        }
      }

    const ActorVector& actors() const {
        return _actors;
    }

    template<class O = YAML::Node, class...Args>
    O get(Args&&...args) {
        return detail::get_helper<O>(std::string{""}, _node, std::forward<Args>(args)...);
    };

private:
    friend class ActorContext;

    ActorVector constructActors(const std::vector<Producer>& producers);
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

    template<class O = YAML::Node, class...Args>
    O get(Args&&...args) {
        return detail::get_helper<O>(std::string{""}, _node, std::forward<Args>(args)...);
    };

    WorkloadContext& workload() {
        return *_workload;
    }

private:
    YAML::Node _node;
    WorkloadContext* _workload;
};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
