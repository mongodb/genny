// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
#define HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/noncopyable.hpp>

#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/ActorVector.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/conventions.hpp>
#include <gennylib/v1/ConfigNode.hpp>
#include <metrics/metrics.hpp>
#include <value_generators/DefaultRandom.hpp>

/**
 * @file context.hpp defines WorkloadContext, ActorContext, and PhaseContext.
 *
 * These provide access to configuration values and other workload collaborators
 * (e.g. metrics) during the construction of actors.
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
 * Given this %YAML:
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
 *     std::optional<int> maybeInt = context.get<int,false>("Actors", 0, "Count");
 * ```
 */
class WorkloadContext : public V1::ConfigNode {
public:
    /**
     * @param node top-level (file-level) YAML node
     * @param registry metrics registry to use in ActorContext::counter() etc
     * @param orchestrator to control Phasing
     * @param mongoUri the base mongo URI to use @see PoolFactory
     * @param cast source of Actors to use. Actors are constructed
     * from the cast at construction-time.
     */
    WorkloadContext(YAML::Node node,
                    metrics::Registry& registry,
                    Orchestrator& orchestrator,
                    const std::string& mongoUri,
                    const Cast& cast);

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

    /**
     * @return a new seeded random number generator.
     * @warning This should only be called during construction to ensure reproducibility.
     */
    auto createRNG() {
        if (_done) {
            throw InvalidConfigurationException(
                "Tried to create a random number generator after construction");
        }
        return DefaultRandom{_rng()};
    }

    /**
     * Get a WorkloadContext-unique ActorId
     * @return The next sequential id
     */
    ActorId nextActorId() {
        return _nextActorId++;
    }

    /**
     * Get states that can be shared across actors using the same WorkloadContext.
     *
     * There is one copy of _state per (ActorT, StateT). It's up to the user to ensure
     * there're not more than one instance of StateT per ActorT to avoid them clobbering
     * each other.
     */
    template <class ActorT, class StateT = typename ActorT::StateT>
    static StateT& getActorSharedState() {
        // C++11 function statics are created in a thread-safe manner.
        static auto _state = StateT();
        return _state;
    }

    /**
     * ShareableState should be the base class of all shareable
     *
     * It uses the "Curiously recurring template" pattern to avoid storing `T` explicitly.
     * Otherwise we'd need to implement a user-defined conversion to T, which
     * would have prevented any further implicit conversions defined by T from being run.
     *
     * @tparam T type of the shareable state.
     */
    template <typename T>
    struct ShareableState : T {
        ShareableState() = default;
        ~ShareableState() = default;
    };

private:
    friend class ActorContext;

    // helper methods used during construction
    static ActorVector _constructActors(const Cast& cast,
                                        const std::unique_ptr<ActorContext>& contexts);

    metrics::Registry* const _registry;
    Orchestrator* const _orchestrator;

    std::unique_ptr<mongocxx::pool> _clientPool;

    // we own the child ActorContexts
    std::vector<std::unique_ptr<ActorContext>> _actorContexts;
    ActorVector _actors;
    DefaultRandom _rng;

    // Indicate that we are doing building the context. This is used to gate certain methods that
    // should not be called after construction.
    bool _done = false;

    // Actors should always be constructed in a single-threaded context.
    // That said, atomic integral types are very cheap to work with.
    std::atomic<ActorId> _nextActorId{0};
};

// For some reason need to decl this; see impl below
class PhaseContext;

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
 * Given this %YAML:
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
class ActorContext final : public V1::ConfigNode {
public:
    ActorContext(YAML::Node node, WorkloadContext& workloadContext)
        : ConfigNode(std::move(node), std::addressof(workloadContext)),
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
     * @return top-level workload configuration
     */
    WorkloadContext& workload() {
        return *this->_workload;
    }

    /**
     * @return the workload-wide Orchestrator
     */
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

    /**
     * @return a pool from the "default" MongoDB connection-pool.
     * @throws InvalidConfigurationException if no connections available.
     */
    mongocxx::pool::entry client();

    // <Forwarding to delegates>

    /**
     * Convenience method for creating a metrics::Timer.
     *
     * @param operationName
     *   the name of the thing being timed.
     *   Will automatically add prefixes to make the full name unique
     *   across Actors and threads.
     * @param id the id of this Actor, if any.
     */
    auto timer(const std::string& operationName, ActorId id = 0u) const {
        auto name = this->metricsName(operationName, id);
        return this->_workload->_registry->timer(name);
    }

    /**
     * Convenience method for creating a metrics::Gauge.
     *
     * @param operationName
     *   the name of the thing being gauged.
     *   Will automatically add prefixes to make the full name unique
     *   across Actors and threads.
     * @param id the id of this Actor, if any.
     */
    auto gauge(const std::string& operationName, ActorId id = 0u) const {
        auto name = this->metricsName(operationName, id);
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
     * @param id the id of this Actor, if any.
     */
    auto counter(const std::string& operationName, ActorId id = 0u) const {
        auto name = this->metricsName(operationName, id);
        return this->_workload->_registry->counter(name);
    }

    auto operation(const std::string& operationName, ActorId id = 0u) const {
        auto name = this->metricsName(operationName, id);
        return this->_workload->_registry->operation(name);
    }

    /**
     * @return if there are any more phases.
     * @see Orchestrator::morePhases()
     */
    auto morePhases() {
        return this->_workload->_orchestrator->morePhases();
    }

    /**
     * @return the current PhaseNumber from the Orchestrator
     * @see Orchestrator::currentPhase()
     */
    auto currentPhase() {
        return this->_workload->_orchestrator->currentPhase();
    }

    /**
     * Block until Phase starts.
     * @return value from Orchestrator::awaitPhaseStart()
     * @see Orchestrator::awaitPhaseStart()
     */
    auto awaitPhaseStart() {
        return this->_workload->_orchestrator->awaitPhaseStart();
    }

    /**
     * Block until Phase ends.
     * @return value from Orchestrator::awaitPhaseEnd()
     * @see Orchestrator::awaitPhaseEnd()
     */
    template <class... Args>
    auto awaitPhaseEnd(Args&&... args) {
        return this->_workload->_orchestrator->awaitPhaseEnd(std::forward<Args>(args)...);
    }

    /**
     * Indicate a problem that should cause all Actors that are using
     * the Orchestrator to abort.
     * @return value from Orchestrator::abort()
     * @see Orchestrator::abort()
     */
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
    std::string metricsName(const std::string& operation, ActorId id) const {
        return this->get<std::string>("Name") + ".id-" + std::to_string(id) + "." + operation;
    }

    static std::unordered_map<genny::PhaseNumber, std::unique_ptr<PhaseContext>>
    constructPhaseContexts(const YAML::Node&, ActorContext*);
    WorkloadContext* _workload;
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> _phaseContexts;
};


class PhaseContext final : public V1::ConfigNode {

public:
    PhaseContext(YAML::Node node, const ActorContext& actorContext)
        : ConfigNode(std::move(node), std::addressof(actorContext)) {}

    // no copy or move
    PhaseContext(PhaseContext&) = delete;
    void operator=(PhaseContext&) = delete;
    PhaseContext(PhaseContext&&) = default;
    void operator=(PhaseContext&&) = delete;

    /**
     * Called in PhaseLoop during the IterationCompletionCheck constructor.
     */
    bool isNop() const {
        auto isNop = _isNop();

        // Check to make sure we haven't broken our rules
        if (isNop && _node.size() > 1) {
            if (_node.size() != 2 || !_node["Phase"]) {
                throw InvalidConfigurationException(
                        "Nop cannot be used with any other keywords except Phase. Check YML configuration.");
            }
        }

        return isNop;
    }

private:
    bool _isNop() const;

private:
    const ActorContext* _actor;
};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
