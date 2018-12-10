#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <cassert>
#include <random>
#include <vector>

#include <boost/noncopyable.hpp>

#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include "yaml_config.hh"
#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/conventions.hpp>
#include <gennylib/metrics.hpp>

/**
 * This file defines `WorkloadContext`, `ActorContext`, and `PhaseContext` which provide access
 * to configuration values and other workload collaborators (e.g. metrics) during the construction
 * of actors.
 *
 * Please see the documentation below on WorkloadContext, ActorContext, and PhaseContext.
 */
namespace genny {

/**
 * Represents the top-level/"global" configuration and context for configuring actors.
 * Call `.get()` to access top-level yaml configs.
 *
 * The get() method is somewhat expensive and should only be called during actor/workload setup.
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
 *
 *     // if value may not exist:
 *     std::optional<int> = context.get<int,false>("Actors", 0, "Count");
 * ```
 */
class WorkloadContext : public V1::ConfigRoot {
public:
    /**
     * @param producers
     *  producers are called eagerly at construction-time.
     */
    WorkloadContext(YAML::Node node,
                    metrics::Registry& registry,
                    Orchestrator& orchestrator,
                    const std::string& mongoUri,
                    const std::vector<ActorProducer>& producers)
        : V1::ConfigRoot(std::move(node)),
          _registry{&registry},
          _orchestrator{&orchestrator},
          // TODO: make this optional and default to mongodb://localhost:27017
          _clientPool{mongocxx::uri{mongoUri}},
          _done{false} {
        // This is good enough for now. Later can add a WorkloadContextValidator concept
        // and wire in a vector of those similar to how we do with the vector of Producers.
        if (this->get_noinherit<std::string>("SchemaVersion") != "2018-07-01") {
            throw InvalidConfigurationException("Invalid schema version");
        }

        // Default value selected from random.org, by selecting 2 random numbers
        // between 1 and 10^9 and concatenating.
        rng.seed(this->get_noinherit<int, false>("RandomSeed").value_or(269849313357703264));
        _actorContexts = constructActorContexts(this);
        _actors = constructActors(producers, _actorContexts);
        _done = true;
    }

    // no copy or move
    WorkloadContext(WorkloadContext&) = delete;
    void operator=(WorkloadContext&) = delete;
    WorkloadContext(WorkloadContext&&) = default;
    void operator=(WorkloadContext&&) = delete;

    /**
     * @return all the actors produced. This should only be called by workload drivers.
     */
    constexpr const ActorVector& actors() const {
        return _actors;
    }

    /*
     * @return a new seeded random number generator. This should only be called during construction
     * to ensure reproducibility.
     */
    std::mt19937_64 createRNG() {
        if (_done) {
            throw InvalidConfigurationException(
                "Tried to create a random number generator after construction");
        }
        return std::mt19937_64{rng()};
    }

private:
    friend class ActorContext;

    // helper methods used during construction
    static ActorVector constructActors(const std::vector<ActorProducer>& producers,
                                       const std::vector<std::unique_ptr<ActorContext>>&);

    std::vector<std::unique_ptr<ActorContext>> constructActorContexts(WorkloadContext*);

    std::mt19937_64 rng;
    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;
    // we own the child ActorContexts
    std::vector<std::unique_ptr<ActorContext>> _actorContexts;
    mongocxx::pool _clientPool;
    ActorVector _actors;
    // Indicate that we are doing building the context. This is used to gate certain methods that
    // should not be called after construction.
    bool _done;
};

// For some reason need to decl this; see impl below
class PhaseContext;
class OperationContext;

/**
 * Represents each `Actor:` block within a WorkloadConfig.
 *
 * The get() method is somewhat expensive and should only be called during actor/workload setup.
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
 * There will be two ActorConfigs, one for `{Name: Foo}` and another for `{Name: Bar}`.
 *
 * ```c++
 * auto name = cx.get<std::string>("Name");
 * ```
 */
class ActorContext final : public V1::ConfigNode<WorkloadContext> {
public:
    ActorContext(YAML::Node node, WorkloadContext& workloadContext)
        : ConfigNode(std::move(node), workloadContext),
          _workload{&workloadContext},
          _phaseContexts{} {
        _phaseContexts = constructPhaseContexts(_node, this);
    }

    // no copy or move
    ActorContext(ActorContext&) = delete;
    void operator=(ActorContext&) = delete;
    ActorContext(ActorContext&&) = default;
    void operator=(ActorContext&&) = delete;

    /**
     * Access top-level workload configuration.
     */
    WorkloadContext& workload() {
        return *this->_workload;
    }

    Orchestrator& orchestrator() {
        return *this->_workload->_orchestrator;
    }

    /**
     * @return a structure representing the `Phases:` block in the Actor config.
     *
     * If you want per-Phase configuration, consider using `PhaseLoop<T>` which
     * will let you construct a `T` for each Phase at constructor-time and will
     * automatically coordinate with the `Orchestrator`.
     *   ** See extended example on the `PhaseLoop` class. **
     *
     * Keys are phase numbers and values are the Phase blocks associated with them.
     * Empty if there are no configured Phases.
     *
     * E.g.
     *
     * ```yaml
     * ...
     * Actors:
     * - Name: Linkbench
     *   Type: Linkbench
     *   Collection: links
     *
     *   Phases:
     *   - Phase: 0
     *     Operation: Insert
     *     Repeat: 1000
     *     # Inherits `Collection: links` from parent
     *
     *   - Phase: 1
     *     Operation: Request
     *     Duration: 1 minute
     *     Collection: links2 # Overrides `Collection: links` from parent
     *
     *   - Operation: Cleanup
     *     # inherits `Collection: links` from parent,
     *     # and `Phase: 3` is derived based on index
     * ```
     *
     * This would result in 3 `PhaseContext` structures. Keys are inherited from the
     * parent (Actor-level) unless overridden, and the `Phase` key is defaulted from
     * the block's index if not otherwise specified.
     *
     * *Note* that Phases are "opt-in" to all Actors and may represent phase-specific
     * configuration in other mechanisms if desired. The `Phases:` structure and
     * related PhaseContext type are purely for conventional convenience.
     */
    const std::unordered_map<genny::PhaseNumber, std::unique_ptr<PhaseContext>>& phases() const {
        return _phaseContexts;
    };

    mongocxx::pool::entry client() {
        auto entry = _workload->_clientPool.try_acquire();
        if (!entry) {
            throw InvalidConfigurationException("Failed to acquire an entry from the client pool.");
        }
        return std::move(*entry);
    }

    // <Forwarding to delegates>

    /**
     * Convenience method for creating a metrics::Timer.
     *
     * @param operationName
     *   the name of the thing being timed.
     *   Will automatically add prefixes to make the full name unique
     *   across Actors and threads.
     * @param thread the thread number of this Actor, if any.
     */
    auto timer(const std::string& operationName, unsigned int thread = 0) const {
        auto name = this->metricsName(operationName, thread);
        return this->_workload->_registry->timer(name);
    }

    /**
     * Convenience method for creating a metrics::Gauge.
     *
     * @param operationName
     *   the name of the thing being gauged.
     *   Will automatically add prefixes to make the full name unique
     *   across Actors and threads.
     * @param thread the thread number of this Actor, if any.
     */
    auto gauge(const std::string& operationName, unsigned int thread = 0) const {
        auto name = this->metricsName(operationName, thread);
        return this->_workload->_registry->gauge(name);
    }

    /**
     * Convenience method for creating a metrics::Counter.
     *
     *
     * @param operationName
     *   the name of the thing being counted.
     *   Will automatically add prefixes to make the full name unique
     *   across Actors and threads.
     * @param thread the thread number of this Actor, if any.
     */
    auto counter(const std::string& operationName, unsigned int thread = 0) const {
        auto name = this->metricsName(operationName, thread);
        return this->_workload->_registry->counter(name);
    }

    auto morePhases() {
        return this->_workload->_orchestrator->morePhases();
    }

    auto currentPhase() {
        return this->_workload->_orchestrator->currentPhase();
    }
    auto awaitPhaseStart() {
        return this->_workload->_orchestrator->awaitPhaseStart();
    }

    template <class... Args>
    auto awaitPhaseEnd(Args&&... args) {
        return this->_workload->_orchestrator->awaitPhaseEnd(std::forward<Args>(args)...);
    }

    auto abort() {
        return this->_workload->_orchestrator->abort();
    }

    // </Forwarding to delegates>

private:
    /**
     * Apply metrics names conventions based on configuration.
     *
     * @param operation base name of a metrics object e.g. "inserts"
     * @param thread the thread number of the Actor owning the object.
     * @return the fully-qualified metrics name e.g. "MyActor.0.inserts".
     */
    std::string metricsName(const std::string& operation, unsigned int thread) const {
        return this->get<std::string>("Name") + "." + std::to_string(thread) + "." + operation;
    }

    static std::unordered_map<genny::PhaseNumber, std::unique_ptr<PhaseContext>>
    constructPhaseContexts(const YAML::Node&, ActorContext*);
    YAML::Node _node;
    WorkloadContext* _workload;
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> _phaseContexts;
};


class PhaseContext final : public V1::ConfigNode<ActorContext> {

public:
    PhaseContext(YAML::Node node, const ActorContext& actorContext)
        : ConfigNode(std::move(node), actorContext) {
        _operationContexts = constructOperationContexts(_node, this);
    }

    // no copy or move
    PhaseContext(PhaseContext&) = delete;
    void operator=(PhaseContext&) = delete;
    PhaseContext(PhaseContext&&) = default;
    void operator=(PhaseContext&&) = delete;

    /**
     * Called in PhaseLoop during the IterationCompletionCheck constructor.
     */
    bool isNop() const {
        bool isNop = get<std::string, false>("Operation") && get<std::string>("Operation") == "Nop";
        if (isNop && _node.size() != 1) {
            throw InvalidConfigurationException(
                "Nop cannot be used with any other keywords. Check YML configuration.");
        }
        return isNop;
    }

    /**
     * @return a structure representing the `Operations:` block in the Phase config.
     *
     * Keys are the metric names and values are the operation contexts associated with the metric.
     * The structure is empty if there are no operations represented.
     */
    const std::unordered_map<std::string, std::unique_ptr<genny::OperationContext>>& operations()
        const {
        return _operationContexts;
    };

private:
    static std::unordered_map<std::string, std::unique_ptr<genny::OperationContext>>
    constructOperationContexts(const YAML::Node&, PhaseContext*);
    std::unordered_map<std::string, std::unique_ptr<genny::OperationContext>> _operationContexts;
};

class OperationContext final : public V1::ConfigNode<PhaseContext> {
public:
    OperationContext(YAML::Node node, const PhaseContext& phaseContext)
        : ConfigNode(std::move(node), phaseContext) {
        std::cout << "node: " << node << std::endl;
    }

    // no copy or move
    OperationContext(OperationContext&) = delete;
    void operator=(OperationContext&) = delete;
    OperationContext(OperationContext&&) = default;
    void operator=(OperationContext&&) = delete;
};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
