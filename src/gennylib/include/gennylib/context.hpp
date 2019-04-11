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
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/throw_exception.hpp>

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
#include <gennylib/v1/GlobalRateLimiter.hpp>
#include <gennylib/v1/PoolManager.hpp>

#include <metrics/metrics.hpp>

#include <value_generators/DefaultRandom.hpp>
#include <value_generators/DocumentGenerator.hpp>

/**
 * @file context.hpp defines WorkloadContext, ActorContext, and PhaseContext.
 *
 * These provide access to configuration values and other workload collaborators
 * (e.g. metrics) during the construction of actors.
 *
 * Please see the documentation below on WorkloadContext, ActorContext, and PhaseContext.
 */
namespace genny {

class WorkloadContext;

namespace v1 {
// pre-declared because compilers are hateful.
// See documentation on ActorContext::createDocumentGenerator
template <typename PathOrNode, typename... Args>
DocumentGenerator createDocumentGeneratorImpl(WorkloadContext& workloadContext,
                                              const ConfigNode& configNode,
                                              ActorId id,
                                              PathOrNode&& pathOrNode,
                                              Args&&... args);
}  // namespace v1

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
class WorkloadContext : public v1::ConfigNode {
public:
    /**
     * @param node top-level (file-level) YAML node
     * @param registry metrics registry to use in ActorContext::counter() etc
     * @param orchestrator to control Phasing
     * @param mongoUri the base mongo URI to use @see PoolFactory
     * @param cast source of Actors to use. Actors are constructed
     * from the cast at construction-time.
     */
    WorkloadContext(const YAML::Node& node,
                    metrics::Registry& registry,
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
     * @warning
     *   This is intended to only be used internally. It is called
     *   by PhaseLoop in response to the `Rate:` yaml keyword.
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
    v1::GlobalRateLimiter* getRateLimiter(const std::string& name, const RateSpec& spec);

private:
    friend class ActorContext;

    // helper methods used during construction
    static ActorVector _constructActors(const Cast& cast,
                                        const std::unique_ptr<ActorContext>& contexts);

    metrics::Registry* _registry;
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
    std::atomic<ActorId> _nextActorId{0};

    std::unordered_map<ActorId, DefaultRandom> _rngRegistry;

    std::unordered_map<std::string, std::unique_ptr<v1::GlobalRateLimiter>> _rateLimiters;
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
class ActorContext final : public v1::ConfigNode {
public:
    ActorContext(const YAML::Node& node, WorkloadContext& workloadContext)
        : ConfigNode(node, std::addressof(workloadContext)),
          _workload{&workloadContext},
          _phaseContexts{} {
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
     * @param id the id of this Actor, if any.
     */
    auto operation(const std::string& operationName, ActorId id) const {
        return this->_workload->_registry->operation(
            this->get<std::string>("Name"), operationName, id);
    }

    /**
     * Construct a DocumentGenerator from a given path or YAML::Node.
     *
     * Example usage:
     *
     * ```yaml
     * SchemaVersion: 2018-07-01
     * Actors:
     * - Type: Foo
     *   Documents: [{a: {^RandomInt: {min: 7, max: 100}}}]
     *   ...
     * ```
     *
     * This could be used via:
     *
     * ```c++
     * DocumentGenerator docGen =
     *     actorContext.createDocumentGenerator(FooActor::id(), "Document", 0);
     * auto document = docGen(); // => {a: random-between-7-and-10}
     * ```
     *
     * The 2nd and 3rd arguments (`"Document",0`) indicate the path to the
     * DocumentGenerator "template" to use.
     *
     * The second argument, `pathOrNode`, can be either a `YAML::Node` or an argument
     * that could be passed to `get()` (i.e. a string or int part of a YAML path as
     * in the example above). If it is a `YAML::Node`, no additional arguments
     * may be given.
     *
     * Example with `YAML::Node` as the second argument:
     *
     * ```c++
     * auto node = YAML::Load("{a: {^RandomInt: {min: 0, max: 5}}}");
     * DocumentGenerator docGen =
     *     actorContext.createDocumentGenerator(MyActor::id(), node);
     * auto document = docGen(); // => {a: random-between-0-and-5}
     * ```
     *
     * In this second example it would be a compiler error to pass
     * additional arguments after `node`:
     *
     * ```c++
     * auto node = YAML::Load("{}");
     * //                                       ☣️ Compiler Error ↓ ☣
     * actorContext.createDocumentGenerator(MyActor::id(), node, "foo");
     * ```
     *
     * Important notes:
     *
     * 1. DocumentGenerators constructed with the same `ActorId`
     *    (the first parameter) will share the same random number
     *    generator and should not be used at the same time on
     *    different threads.
     *
     * 2. This can only be called during workload setup (not within
     *    the `run()` method of Actors).
     *
     * @param id
     *   the ActorId that will own this DocumentGenerator.
     *   In general use YourActorClass::id()
     * @param pathOrNode
     *   Either a `YAML::Node` or the first part of the path
     *   to the structure to use for the document template.
     *   See `get()`.
     * @param args
     *   Remaining path elements. Can only be given if
     *   `pathOrNode` is *not* a `YAML::Node`.
     * @return
     *   A hot and fresh DocumentGenerator
     */
    template <typename PathOrNode, typename... Args>
    DocumentGenerator createDocumentGenerator(ActorId id, PathOrNode&& pathOrNode, Args&&... args) {
        return v1::createDocumentGeneratorImpl(this->workload(),
                                               *this,
                                               id,
                                               std::forward<PathOrNode>(pathOrNode),
                                               std::forward<Args>(args)...);
    }

private:
    static std::unordered_map<genny::PhaseNumber, std::unique_ptr<PhaseContext>>

    constructPhaseContexts(const YAML::Node&, ActorContext*);

    WorkloadContext* _workload;
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> _phaseContexts;
};

/**
 * Represents each `Phase:` block in the YAML configuration.
 */
class PhaseContext final : public v1::ConfigNode {

public:
    PhaseContext(const YAML::Node& node, ActorContext& actorContext)
        : ConfigNode(node, std::addressof(actorContext)), _actor{std::addressof(actorContext)} {}

    // no copy or move
    PhaseContext(PhaseContext&) = delete;
    void operator=(PhaseContext&) = delete;
    PhaseContext(PhaseContext&&) = delete;
    void operator=(PhaseContext&&) = delete;

    /** See documentation on ActorContext::createDocumentGenerator() */
    template <typename PathOrNode, typename... Args>
    DocumentGenerator createDocumentGenerator(ActorId id, PathOrNode&& pathOrNode, Args&&... args) {
        return v1::createDocumentGeneratorImpl(this->workload(),
                                               *this,
                                               id,
                                               std::forward<PathOrNode>(pathOrNode),
                                               std::forward<Args>(args)...);
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

private:
    bool _isNop() const;

private:
    ActorContext* _actor;
};

namespace v1 {


/**
 * Can we convert A to B ignoring cv qualification and ref-ness of A?
 */
template <typename A, typename B>
using IsLooselyConvertible = std::is_convertible<std::remove_reference_t<std::remove_cv_t<A>>, B>;

template <typename PathOrNode, typename... Args>
DocumentGenerator createDocumentGeneratorImpl(genny::WorkloadContext& workloadContext,
                                              const ConfigNode& configNode,
                                              ActorId id,
                                              PathOrNode&& pathOrNode,
                                              Args&&... args) {
    if (workloadContext.isDone()) {
        std::stringstream msg;
        msg << "Tried to create DocumentGenerator";
        if constexpr (!IsLooselyConvertible<PathOrNode, YAML::Node>::value) {
            msg << " [";
            msg << pathOrNode;
            (msg << ... << args) << "]";
        }
        msg << " after workload setup completed.";
        BOOST_THROW_EXCEPTION(std::logic_error(msg.str()));
    }

    DefaultRandom& rng = workloadContext.getRNGForThread(id);

    if constexpr (IsLooselyConvertible<PathOrNode, YAML::Node>::value) {
        // If we're calling via context.createDocGen(id, YAML::Node)

        // It's undefined what this would even mean so may as well
        // protect against bad usage.
        static_assert(sizeof...(args) == 0,
                      "When calling createDocumentGenerator(id,YAML::Node) "
                      "there cannot be additional arguments");

        return DocumentGenerator{pathOrNode, rng};
    } else {
        auto node =
            configNode.get(std::forward<PathOrNode>(pathOrNode), std::forward<Args>(args)...);
        return DocumentGenerator{node, rng};
    }
}

}  // namespace v1


}  // namespace genny

#endif  // HEADER_0E802987_B910_4661_8FAB_8B952A1E453B_INCLUDED
