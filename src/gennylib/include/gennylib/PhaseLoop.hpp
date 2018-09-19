#ifndef HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
#define HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED

#include <cassert>
#include <chrono>
#include <iterator>
#include <optional>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>

/**
 * @file
 * This file provides the `PhaseLoop<T>` type and the collaborator classes that make it iterable.
 * See extended example on the PhaseLoop class docs.
 */

namespace genny {

/*
 * Reminder: the V1 namespace types are *not* intended to be used directly.
 */
namespace V1 {

/**
 * Determine if we're done iterating for a given Phase.
 */
class IterationCompletionCheck final {

public:
    explicit IterationCompletionCheck() : IterationCompletionCheck(std::nullopt, std::nullopt) {}

    IterationCompletionCheck(std::optional<std::chrono::milliseconds> minDuration,
                             std::optional<int> minIterations)
        : _minDuration{minDuration},
          _minIterations{minIterations},
          _doesBlock{_minIterations || _minDuration} {

        if (minDuration && minDuration->count() < 0) {
            std::stringstream str;
            str << "Need non-negative duration. Gave " << minDuration->count() << " milliseconds";
            throw InvalidConfigurationException(str.str());
        }
        if (minIterations && *minIterations < 0) {
            std::stringstream str;
            str << "Need non-negative number of iterations. Gave " << *minIterations;
            throw InvalidConfigurationException(str.str());
        }
    }

    explicit IterationCompletionCheck(PhaseContext& phaseContext)
        : IterationCompletionCheck(phaseContext.get<std::chrono::milliseconds, false>("Duration"),
                                   phaseContext.get<int, false>("Repeat")) {}

    std::chrono::steady_clock::time_point referenceStartingPoint() const {
        // avoid doing now() if no minDuration configured
        return _minDuration ? std::chrono::steady_clock::now()
                            : std::chrono::time_point<std::chrono::steady_clock>::min();
    }

    bool isDone(std::chrono::steady_clock::time_point startedAt,
                unsigned int currentIteration) const {
        return (!_minIterations || currentIteration >= *_minIterations) &&
            (!_minDuration ||
             // check is last to avoid doing now() call unnecessarily
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - startedAt) >= *_minDuration);
    }

    bool operator==(const IterationCompletionCheck& other) const {
        return _minDuration == other._minDuration && _minIterations == other._minIterations;
    }

    bool doesBlock() const {
        return _doesBlock;
    }

private:
    // Debatable about whether this should also track the current iteration and
    // referenceStartingPoint time (versus having those in the ActorPhaseIterator). BUT: even the
    // .end() iterator needs an instance of this, so it's weird

    const std::optional<std::chrono::milliseconds> _minDuration;
    const std::optional<int> _minIterations;
    const bool _doesBlock;  // Computed/cached value. Computed at ctor time.
};


/**
 * The iterator used in `for(auto _ : phase)` and returned from
 * `ActorPhase::begin()` and `ActorPhase::end()`.
 *
 * Configured with {@link IterationCompletionCheck} and will continue
 * iterating until configured #iterations or duration are done
 * or, if non-blocking, when Orchestrator says phase has changed.
 */
class ActorPhaseIterator final {

public:
    ActorPhaseIterator(Orchestrator& orchestrator,
                       const IterationCompletionCheck* iterCheck,
                       PhaseNumber inPhase,
                       bool isEndIterator)
        : _orchestrator{std::addressof(orchestrator)},
          _iterCheck{iterCheck},
          _referenceStartingPoint{isEndIterator
                                      ? std::chrono::time_point<std::chrono::steady_clock>::min()
                                      : _iterCheck->referenceStartingPoint()},
          _inPhase{inPhase},
          _isEndIterator{isEndIterator},
          _currentIteration{0} {}

    ActorPhaseIterator(Orchestrator& orchestrator, PhaseNumber inPhase, bool isEnd)
        : ActorPhaseIterator{orchestrator, nullptr, inPhase, isEnd} {}

    // iterator concept value-type
    // intentionally empty; most compilers will elide any actual storage
    struct Value {};

    Value operator*() const {
        return Value();
    }

    ActorPhaseIterator& operator++() {
        ++_currentIteration;
        return *this;
    }

    // clang-format off
    bool operator==(const ActorPhaseIterator& rhs) const {
        return
                // we're comparing against the .end() iterator (the common case)
                (rhs._isEndIterator && !this->_isEndIterator &&
                   // if we block, then check to see if we're done in current phase
                   // else check to see if current phase has expired
                   (_iterCheck->doesBlock() ? _iterCheck->isDone(_referenceStartingPoint, _currentIteration)
                                            : _orchestrator->currentPhase() != _inPhase))

                // Below checks are mostly for pure correctness;
                //   "well-formed" code will only use this iterator in range-based for-loops and will thus
                //   never use these conditions.
                //
                //   Could probably put these checks under a debug-build flag or something?

                // this == this
                || (this == &rhs)

                // neither is end iterator but have same fields
                || (!rhs._isEndIterator && !_isEndIterator
                    && _referenceStartingPoint  == rhs._referenceStartingPoint
                    && _currentIteration        == rhs._currentIteration
                    && _iterCheck               == rhs._iterCheck)

                // both .end() iterators (all .end() iterators are ==)
                || (_isEndIterator && rhs._isEndIterator)

                // we're .end(), so 'recurse' but flip the args so 'this' is rhs
                || (_isEndIterator && rhs == *this);
    }
    // clang-format on

    // Iterator concepts only require !=, but the logic is much easier to reason about
    // for ==, so just negate that logic 😎 (compiler should inline it)
    bool operator!=(const ActorPhaseIterator& rhs) const {
        return !(*this == rhs);
    }

private:
    Orchestrator* _orchestrator;
    const IterationCompletionCheck* _iterCheck;
    const std::chrono::steady_clock::time_point _referenceStartingPoint;
    const PhaseNumber _inPhase;
    const bool _isEndIterator;
    unsigned int _currentIteration;

public:
    // <iterator-concept>
    typedef std::forward_iterator_tag iterator_category;
    typedef Value value_type;
    typedef Value reference;
    typedef Value pointer;
    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>
};


/**
 * Represents an Actor's configuration for a particular Phase.
 *
 * Its iterator, `ActorPhaseIterator`, lets Actors do an operation in a loop
 * for a pre-determined number of iterations or duration or,
 * if the Phase is non-blocking for the Actor, as long as the
 * Phase is held open by other Actors.
 *
 * This is intended to be used via `PhaseLoop` below.
 */
template <class T>
class ActorPhase final {

public:
    template <class... Args>
    ActorPhase(Orchestrator& orchestrator,
               std::unique_ptr<const IterationCompletionCheck> iterCheck,
               PhaseNumber inPhase,
               Args... args)
        : _orchestrator{orchestrator},
          _iterCheck{std::move(iterCheck)},
          _inPhase{inPhase},
          _value{std::make_unique<T>(std::forward<Args>(args)...)} {}

    template <class... Args>
    ActorPhase(Orchestrator& orchestrator,
               PhaseContext& phaseContext,
               PhaseNumber inPhase,
               Args&&... args)
        : _orchestrator{orchestrator},
          _iterCheck{std::make_unique<IterationCompletionCheck>(phaseContext)},
          _inPhase{inPhase},
          _value{std::make_unique<T>(std::forward<Args>(args)...)} {}

    ActorPhaseIterator begin() {
        return ActorPhaseIterator{_orchestrator, _iterCheck.get(), _inPhase, false};
    }

    ActorPhaseIterator end() {
        return ActorPhaseIterator{_orchestrator, _inPhase, true};
    };

    // Used by PhaseLoopIterator::doesBlock()
    bool doesBlock() const {
        return _iterCheck->doesBlock();
    }

    // could use `auto` for return-type of operator-> and operator*, but
    // IDE auto-completion likes it more if it's spelled out.
    typename std::add_pointer_t<std::remove_reference_t<T>> operator->() const noexcept {
        return _value.operator->();
    }

    typename std::add_lvalue_reference_t<T> operator*() const {
        return _value.operator*();
    }

private:
    Orchestrator& _orchestrator;
    std::unique_ptr<const IterationCompletionCheck> _iterCheck;
    const PhaseNumber _inPhase;
    std::unique_ptr<T> _value;

};  // class ActorPhase


/**
 * Maps from PhaseNumber to the ActorPhase<T> to be used in that PhaseNumber.
 */
template <class T>
using PhaseMap = std::unordered_map<PhaseNumber, V1::ActorPhase<T>>;


/**
 * The iterator used by `for(auto&& [p,h] : phaseLoop)`.
 *
 * @attention This type is only intended to be used by range-based for loops.
 *            Other STL algorithms like `std::advance` etc. are not supported to work.
 *
 * @tparam T the per-Phase type that will be exposed for each Phase.
 *
 * Iterates over all phases and will correctly call
 * `awaitPhaseStart()` and `awaitPhaseEnd()` in the
 * correct operators.
 */
template <class T>
class PhaseLoopIterator final {

public:
    PhaseLoopIterator(Orchestrator& orchestrator, PhaseMap<T>& phaseMap, bool isEnd)
        : _orchestrator{orchestrator},
          _phaseMap{phaseMap},
          _isEnd{isEnd},
          _currentPhase{0},
          _awaitingPlusPlus{false} {}

    std::pair<PhaseNumber, ActorPhase<T>&> operator*() /* cannot be const */ {
        assert(!_awaitingPlusPlus);
        // Intentionally don't bother with cases where user didn't call operator++()
        // between invocations of operator*() and vice-versa.
        _currentPhase = this->_orchestrator.awaitPhaseStart();
        if (!this->doesBlockOn(_currentPhase)) {
            this->_orchestrator.awaitPhaseEnd(false);
        }

        _awaitingPlusPlus = true;

        auto&& found = _phaseMap.find(_currentPhase);

        if (found == _phaseMap.end()) {
            // We're (incorrectly) constructed outside of the conventional flow,
            // i.e., the `PhaseLoop(ActorContext&)` ctor. Could also happen if Actors
            // are configured with different sets of Phase numbers.
            std::stringstream msg;
            msg << "No phase config found for PhaseNumber=[" << _currentPhase << "]";
            throw InvalidConfigurationException(msg.str());
        }

        return {_currentPhase, found->second};
    }

    PhaseLoopIterator& operator++() {
        assert(_awaitingPlusPlus);
        // Intentionally don't bother with cases where user didn't call operator++()
        // between invocations of operator*() and vice-versa.
        if (this->doesBlockOn(_currentPhase)) {
            this->_orchestrator.awaitPhaseEnd(true);
        }

        _awaitingPlusPlus = false;
        return *this;
    }

    bool operator!=(const PhaseLoopIterator& other) const {
        // Intentionally don't handle self-equality or other "normal" cases.
        return !(other._isEnd && !this->morePhases());
    }

private:
    bool morePhases() const {
        return this->_orchestrator.morePhases();
    }

    bool doesBlockOn(PhaseNumber phase) const {
        if (auto h = _phaseMap.find(phase); h != _phaseMap.end()) {
            return h->second.doesBlock();
        }
        return true;
    }

    Orchestrator& _orchestrator;
    PhaseMap<T>& _phaseMap;  // cannot be const; owned by PhaseLoop

    const bool _isEnd;
    PhaseNumber _currentPhase;

    // helps detect accidental mis-use. General contract
    // of this iterator (as used by range-based for) is that
    // the user will alternate between operator*() and operator++()
    // (starting with operator*()), so we flip this back-and-forth
    // in operator*() and operator++() and assert the correct value.
    // If the user calls operator*() twice without calling operator++()
    // between, we'll fail (and similarly for operator++()).
    bool _awaitingPlusPlus;

    // These are intentionally commented-out because this type
    // should not be used by any std algorithms that may rely on them.
    // This type should only be used by range-based for loops (which doesn't
    // rely on these typedefs). This should *hopefully* prevent some cases of
    // accidental mis-use.
    //
    // Decided to leave this code commented-out rather than deleting it
    // partially to document this shortcoming explicitly but also in case
    // we want to support the full concept in the future.
    // https://en.cppreference.com/w/cpp/named_req/InputIterator
    //
    // <iterator-concept>
    //    typedef std::forward_iterator_tag iterator_category;
    //    typedef PhaseNumber value_type;
    //    typedef PhaseNumber reference;
    //    typedef PhaseNumber pointer;
    //    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

};  // class PhaseLoopIterator

}  // namespace V1


/**
 * @return an object that iterates over all configured Phases, calling `awaitPhaseStart()`
 *         and `awaitPhaseEnd()` at the appropriate times. The value-type, `ActorPhase`
 *         is also iterable so your Actor can loop for the entire duration of the Phase.
 *
 * Note that `PhaseLoop`s are relatively expensive to construct and should be constructed
 * at actor-constructor time.
 *
 * Example usage:
 *
 * ```c++
 *     struct MyActor : public Actor {
 *
 *     private:
 *         // Actor-private struct that the Actor uses to determine what
 *         // to do for each Phase. Likely holds ValueGenerators or other
 *         // expensive-to-construct objects. PhaseLoop will construct these
 *         // at Actor setup time rather than at runtime.
 *         struct MyActorConfig {
 *             int _index;
 *             // Must have a ctor that takes a PhaseConfig&
 *             MyActorConfig(PhaseConfig& phaseConfig)
 *             : _index{phaseConfig.get<int>("Index")} {}
 *         };
 *
 *         PhaseLoop<MyActorConfig> _loop;
 *
 *     public:
 *         MyActor(ActorContext& actorContext)
 *         : _loop{actorContext} {}
 *
 *         void run() {
 *             for(auto&& [phaseNum, actorPhase] : _loop) {     // (1)
 *                 // Access the MyActorConfig for the Phase
 *                 // by using operator->() or operator*().
 *                 auto index = actorPhase->_index;
 *
 *                 // The actorPhase itself is iterable
 *                 // this loop will continue running as long
 *                 // as required per configuration conventions.
 *                 for(auto&& _ : actorPhase) {                 // (2)
 *                     doOperation(actorPhase);
 *                 }
 *             }
 *         }
 *     };
 * ```
 *
 * Internal note:
 * (1) is implemented using PhaseLoop and PhaseLoopIterator.
 * (2) is implemented using ActorPhase and ActorPhaseIterator.
 *
 */
template <class T>
class PhaseLoop final {

public:
    template <class... Args>
    explicit PhaseLoop(ActorContext& context, Args&&... args)
        : PhaseLoop(context.orchestrator(),
                    std::move(constructPhaseMap(context, std::forward<Args>(args)...))) {}

    // Only visible for testing
    PhaseLoop(Orchestrator& orchestrator, V1::PhaseMap<T> phaseMap)
        : _orchestrator{orchestrator}, _phaseMap{std::move(phaseMap)} {
        // propagate this Actor's set up PhaseNumbers to Orchestrator
        for (auto&& [phaseNum, actorPhase] : _phaseMap) {
            orchestrator.phasesAtLeastTo(phaseNum);
        }
    }

    V1::PhaseLoopIterator<T> begin() {
        return V1::PhaseLoopIterator<T>{this->_orchestrator, this->_phaseMap, false};
    }

    V1::PhaseLoopIterator<T> end() {
        return V1::PhaseLoopIterator<T>{this->_orchestrator, this->_phaseMap, true};
    }

private:
    template <class... Args>
    static V1::PhaseMap<T> constructPhaseMap(ActorContext& actorContext, Args&&... args) {

        // clang-format off
        static_assert(std::is_constructible_v<T, PhaseContext&, Args...>);
        static_assert(std::is_constructible_v<V1::ActorPhase<T>, Orchestrator&, PhaseContext&, PhaseNumber, PhaseContext&, Args...>);
        // clang-format on

        V1::PhaseMap<T> out;
        for (auto&& [num, phaseContext] : actorContext.phases()) {
            out.try_emplace(
                // key
                num,
                // args to ActorPhase<T> ctor:
                actorContext.orchestrator(),
                *phaseContext,
                num,
                // last arg(s) get forwarded to T ctor (via forward inside of make_unique)
                *phaseContext,
                std::forward<Args>(args)...);
        }
        return out;
    }

    Orchestrator& _orchestrator;
    V1::PhaseMap<T> _phaseMap;  // we own it
    // _phaseMap cannot be const since we don't want to enforce that the wrapped u_p<T> is const

};  // class PhaseLoop


}  // namespace genny

#endif  // HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
