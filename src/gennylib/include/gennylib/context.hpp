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
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/throw_exception.hpp>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/ActorVector.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/GlobalRateLimiter.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Node.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/conventions.hpp>
#include <gennylib/v1/PoolManager.hpp>

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

namespace v1 {
class HasNode {
public:
    explicit HasNode(const Node& node) : _node{node} {}

    template <typename... Args>
    auto& get(Args&&... args) const {
        return this->_node.operator[](std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto& operator[](Args&&... args) const {
        return this->_node.operator[](std::forward<Args>(args)...);
    }

    template <typename T, typename F = std::function<T(const Node&)>>
    auto getPlural(
        const std::string& singular,
        const std::string& plural,
        // Default conversion function is `node.to<T>()`.
        F&& f = [](const Node& n) { return n.to<T>(); }) {
        return std::move(this->_node.getPlural<T, F>(singular, plural, std::forward<F>(f)));
    }

    auto path() const {
        return _node.path();
    }

protected:
    const Node& _node;
};

}  // namespace v1

class WorkloadContext;

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
class WorkloadContext : public v1::HasNode {
public:
    /**
     * @param node top-level (file-level) YAML node
     * @param registry metrics registry to use in ActorContext::counter() etc
     * @param orchestrator to control Phasing
     * @param mongoUri the base mongo URI to use @see PoolFactory
     * @param cast source of Actors to use. Actors are constructed
     * from the cast at construction-time.
     */
    WorkloadContext(const Node& node,
                    Orchestrator& orchestrator,
                    const std::string& mongoUri,
                    const Cast& cast,
                    v1::PoolManager::OnCommandStartCallback apmCallback = {});

    // no copy or move
    WorkloadContext(WorkloadContext&) = delete;
    void operator=(WorkloadContext&) = delete;
    WorkloadContext(WorkloadContext&&) = delete;
    void operator=(WorkloadContext&&) = delete;

    /**
     * @return all the actors produced. This should only be called by workload drivers.
     */
    constexpr const ActorVector& actors() const {
        return _actors;
    }

    /**
     * @return
     *   *the* DefaultRandom instance for the given `id`.
     *   Note that `DefaultRandom` is *not* thread-safe so two Actors
     *   should not use the same `DefaultRandom` at the same time.
     *   If you use `YourActorClass::id()` for `id` you'll be fine.
     */
    DefaultRandom& getRNGForThread(ActorId id);

    /**
     * @return if we're done constructing the WorkloadContext.
     * Beyond this point no further accesses should be done to various *Context
     * methods (this is loosely enforced).
     */
    bool isDone() const {
        return _done;
    }

    /**
     * Get a WorkloadContext-unique ActorId
     * @return The next sequential id
     */
    ActorId nextActorId() {
        return _nextActorId++;
    }

    /**
     * Return a named connection pool instance.
     *
     * @warning
     *   it is advised to only call this during setup since creating a connection pool
     *   can be an expensive operation
     *
     * @param name
     *   the named pool to use. Corresponds to a key in the `Clients:` configuration keyword.
     * @param instance
     *   which instance of the pool to use
     * @return
     *   a pool from the given MongoDB connection-pool. Pools are created on-demand.
     * @throws
     *   InvalidConfigurationException if no connections available.
     */
    mongocxx::pool::entry client(const std::string& name = "Default", size_t instance = 0);

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
    struct ShareableState : public T {
        ShareableState() = default;
        ~ShareableState() = default;
    };

    /**
     * Access global rate-limiters.
     *
     * It is called by PhaseLoop in response to the `GlobalRate:` yaml keyword. Additionally it
     * cannot be called after the WorkloadContext has been constructed: it can only be called during
     * Actors' constructors, etc.
     *
     * @param name
     *   name/id to use
     * @param spec
     *   rate spec to use if creating a new instance. it is undefined what will
     *   be returned if the getRateLimiter() is called twice with the same name but with different
     *   ratespecs.
     * @return
     *   the existing Subsequent calls with the same name will return the same instance.
     *
     * @private
     */
    GlobalRateLimiter* getRateLimiter(const std::string& name, const RateSpec& spec);

    metrics::Registry& getMetrics() {
        return _registry;
    }

private:
    friend class ActorContext;
    friend class PhaseContext;

    // helper methods used during construction
    static ActorVector _constructActors(const Cast& cast,
                                        const std::unique_ptr<ActorContext>& contexts);

    metrics::Registry _registry;
    Orchestrator* _orchestrator;

    v1::PoolManager _poolManager;

    // we own the child ActorContexts
    std::vector<std::unique_ptr<ActorContext>> _actorContexts;
    ActorVector _actors;
    DefaultRandom _rng;

    // Indicate that we are doing building the context. This is used to gate certain methods that
    // should not be called after construction.
    bool _done = false;

    // Actors should always be constructed in a single-threaded context.
    // That said, atomic integral types are very cheap to work with.
    //
    // We start at 1 because, if we send ID 0 to Poplar, the field
    // gets used as a monotonically-increasing value.
    std::atomic<ActorId> _nextActorId{1};

    std::unordered_map<ActorId, DefaultRandom> _rngRegistry;

    std::unordered_map<std::string, std::unique_ptr<GlobalRateLimiter>> _rateLimiters;
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
class ActorContext final : public v1::HasNode {
public:
    ActorContext(const Node& node, WorkloadContext& workloadContext)
        : v1::HasNode{node}, _workload{&workloadContext}, _phaseContexts{} {
        _phaseContexts = constructPhaseContexts(_node, this);
    }

    // no copy or move
    ActorContext(ActorContext&) = delete;
    void operator=(ActorContext&) = delete;
    ActorContext(ActorContext&&) = delete;
    void operator=(ActorContext&&) = delete;

    /**
     * @return top-level workload configuration
     */
    constexpr WorkloadContext& workload() const {
        return *this->_workload;
    }

    /**
     * @return the workload-wide Orchestrator
     */
    constexpr Orchestrator& orchestrator() const {
        return *this->workload()._orchestrator;
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
    constexpr const std::unordered_map<genny::PhaseNumber, std::unique_ptr<PhaseContext>>& phases()
        const {
        return _phaseContexts;
    }

    DefaultRandom& rng(ActorId id) {
        return this->workload().getRNGForThread(id);
    }

    /**
     * @return a pool from the "default" MongoDB connection-pool.
     * @throws InvalidConfigurationException if no connections available.
     */
    template <class... Args>
    mongocxx::pool::entry client(Args&&... args) {
        return this->_workload->client(std::forward<Args>(args)...);
    }

    /**
     * Convenience method for creating a metrics::Operation that's unique for this actor and thread.
     *
     * @param operationName the name of the operation being run.
     * @param id the id of this Actor.
     * @param internal whether this operation is Genny-internal.
     */
    auto operation(const std::string& operationName, ActorId id, bool internal = false) const {
        return this->_workload->_registry.operation(
            this->_node["Name"].to<std::string>(), operationName, id, std::nullopt, internal);
    }

private:
    static std::unordered_map<genny::PhaseNumber, std::unique_ptr<PhaseContext>>

    constructPhaseContexts(const Node&, ActorContext*);

    WorkloadContext* _workload;
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> _phaseContexts;
};

/**
 * Helper class that provides functions needed to sleep in the current phase.
 */
class SleepContext {
public:
    SleepContext(PhaseNumber phase, const Orchestrator& orchestrator)
        : _phase{phase}, _orchestrator{orchestrator} {}

    void sleep_for(Duration sleep_duration) const;

private:
    PhaseNumber _phase;
    const Orchestrator& _orchestrator;
};

/**
 * Represents each `Phase:` block in the YAML configuration.
 */
class PhaseContext final : public v1::HasNode {
public:
    PhaseContext(const Node& node, PhaseNumber phaseNumber, ActorContext& actorContext)
        : v1::HasNode{node}, _actor{std::addressof(actorContext)}, _phaseNumber(phaseNumber) {}

    // no copy or move
    PhaseContext(PhaseContext&) = delete;
    void operator=(PhaseContext&) = delete;
    PhaseContext(PhaseContext&&) = delete;
    void operator=(PhaseContext&&) = delete;

    DefaultRandom& rng(ActorId id) {
        return this->_actor->rng(id);
    }

    /**
     * Called in PhaseLoop during the IterationCompletionCheck constructor.
     */
    bool isNop() const;

    /**
     * @return the parent workload context
     */
    WorkloadContext& workload() const {
        return _actor->workload();
    }

    ActorContext& actor() const {
        return *_actor;
    }

    SleepContext getSleepContext() const {
        return SleepContext(_phaseNumber, this->actor().orchestrator());
    }

    /**
     * Convenience method for creating a metrics::Operation that's unique for this phase and thread.
     *
     * If "MetricsName" is specified for a phase, it is used.
     * Otherwise "[defaultMetricsName].[phaseNumber]" is used.
     *
     * @param defaultMetricName the default name of the metric if "MetricsName" is not specified
     *                          for a phase in the workload YAML.
     * @param id the id of this Actor.
     * @param internal whether this operation is Genny-internal.
     */
    auto operation(const std::string& defaultMetricsName, ActorId id, bool internal = false) const {
        std::ostringstream stm;
        if (auto metricsName = this->_node["MetricsName"].maybe<std::string>()) {
            stm << *metricsName;
        } else {
            stm << defaultMetricsName << "." << _phaseNumber;
        }

        return this->workload()._registry.operation(
            this->_actor->operator[]("Name").to<std::string>(),
            stm.str(),
            id,
            _phaseNumber,
            internal);
    }

    const auto getPhaseNumber() const {
        return _phaseNumber;
    }

private:
    ActorContext* _actor;
    const PhaseNumber _phaseNumber;
};

}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
