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

/*
 * General TODO:
 *
 * - doc it like it's hot
 *   - each class here, even the dumb helper ones
 *   - some connection with context.hpp to make this class discoverable
 *
 * - can PhaseLoop be pre-declared or something such that the user can
 *   actually do something like `PhaseLoop<MyStruct> loop = actorContext.loop<MyStruct>()`?
 *
 * - Run everything thru the sanitizers
 *
 * - Ensure these classes all follow guidelines in CONTRIBUTING.md
 *
 * - Next ticket
 *   - kill PhasedActor
 *   - Maybe: only check orchestrator.currentPhase() == currentPhase every N iterations since
 *     overhead could be kinda high (like 1/2 as slow in casual observations)
 */

namespace genny {

/*
 * Reminder: the V1 namespace types are *not* intended to be used directly.
 */
namespace V1 {



class IterationCompletionCheck final {

public:
    explicit IterationCompletionCheck() : IterationCompletionCheck(std::nullopt, std::nullopt) {}

    IterationCompletionCheck(std::optional<int> minIterations,
                             std::optional<std::chrono::milliseconds> minDuration)
        : _minDuration{minDuration}, _minIterations{minIterations} {

        if (minIterations && *minIterations < 0) {
            std::stringstream str;
            str << "Need non-negative number of iterations. Gave " << *minIterations;
            throw InvalidConfigurationException(str.str());
        }
        if (minDuration && minDuration->count() < 0) {
            std::stringstream str;
            str << "Need non-negative duration. Gave " << minDuration->count() << " milliseconds";
            throw InvalidConfigurationException(str.str());
        }
    }

    explicit IterationCompletionCheck(PhaseContext& phaseContext)
        : IterationCompletionCheck(phaseContext.get<int, false>("Repeat"),
                                   phaseContext.get<std::chrono::milliseconds, false>("Duration")) {
    }

    std::chrono::steady_clock::time_point startedAt() const {
        // avoid doing now() if no minDuration configured
        return _minDuration ? std::chrono::steady_clock::now()
                            : std::chrono::time_point<std::chrono::steady_clock>::min();
    }

    bool isDone(unsigned int currentIteration,
                std::chrono::steady_clock::time_point startedAt) const {
        return (!_minIterations || currentIteration >= *_minIterations) &&
            (!_minDuration ||
             // check is last to avoid doing now() call unnecessarily
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - startedAt) >= *_minDuration);
    }

    bool operator==(const IterationCompletionCheck& other) const {
        return _minDuration == other._minDuration && _minIterations == other._minIterations;
    }

    // TODO: compute at ctor time
    bool doesBlock() const {
        return _minIterations || _minDuration;
    }

private:
    // Debatable about whether this should also track the current iteration and startedAt time
    // (versus having those in the ActorPhaseIterator).
    // BUT: even the .end() iterator needs an instance of this, so it's weird

    const std::optional<std::chrono::milliseconds> _minDuration;
    const std::optional<int> _minIterations;
};

/**
 * Configured with {@link IterationCompletionCheck} and will continue
 * iterating until configured #iterations or duration are done
 * or, if non-blocking, when Orchestrator says phase has changed.
 */
class ActorPhaseIterator final {

public:
    // iterator concept value-type
    // intentionally empty; most compilers will elide any actual storage
    struct Value {};

    ActorPhaseIterator(Orchestrator& orchestrator,
                       const IterationCompletionCheck& iterCheck,
                       bool isEndIterator,
                       PhaseNumber inPhase)
        : _orchestrator{orchestrator},
          _iterCheck{iterCheck},
          _startedAt{_iterCheck.startedAt()},
          _isEndIterator{isEndIterator},
          _currentIteration{0},
          _inPhase{inPhase} {}

    ActorPhaseIterator(Orchestrator& orchestrator, bool isEnd, PhaseNumber inPhase)
        : ActorPhaseIterator{orchestrator, IterationCompletionCheck{}, isEnd, inPhase} {}

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
                (rhs._isEndIterator &&
                   // if we block, then check to see if we're done in current phase
                   // else check to see if current phase has expired
                   (_iterCheck.doesBlock() ? _iterCheck.isDone(_currentIteration, _startedAt)
                                           : _orchestrator.currentPhase() != _inPhase))

                // Below checks are mostly for pure correctness;
                //   "well-formed" code will only use this iterator in range-based for-loops and will thus
                //   never use these conditions.
                //
                //   Could probably put these checks under a debug-build flag or something?

                // this == this
                || (this == &rhs)

                // neither is end iterator but have same fields
                || (!rhs._isEndIterator && !_isEndIterator
                    && _startedAt        == rhs._startedAt
                    && _currentIteration == rhs._currentIteration
                    && _iterCheck        == rhs._iterCheck)

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
    Orchestrator& _orchestrator;
    const IterationCompletionCheck& _iterCheck;
    const std::chrono::steady_clock::time_point _startedAt;
    const bool _isEndIterator;
    unsigned int _currentIteration;
    const PhaseNumber _inPhase;

public:
    // <iterator-concept>
    typedef std::forward_iterator_tag iterator_category;
    typedef Value value_type;
    typedef Value reference;
    typedef Value pointer;
    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

};


template <class T>
class ActorPhase final {

public:
    template <class... Args>
    ActorPhase(Orchestrator& orchestrator, IterationCompletionCheck iterCheck, PhaseNumber inPhase, Args... args)
        : _orchestrator{orchestrator},
          _iterCheck{std::move(iterCheck)},
          _value{std::make_unique<T>(std::forward<Args>(args)...)},
          _inPhase{inPhase} {}

    template <class... Args>
    ActorPhase(Orchestrator& orchestrator, PhaseContext& phaseContext, PhaseNumber inPhase, Args&&... args)
        : _orchestrator{orchestrator},
          _iterCheck{phaseContext},
          _value{std::make_unique<T>(std::forward<Args>(args)...)},
          _inPhase{inPhase} {}

    ActorPhaseIterator begin() {
        return ActorPhaseIterator{_orchestrator, _iterCheck, false, _inPhase};
    }

    ActorPhaseIterator end() {
        // TODO: change order; _inPhase should be right below isEnd (probably)
        return ActorPhaseIterator{_orchestrator, true, _inPhase};
    };

    // Used by PhaseLoopIterator::doesBlock()
    bool doesBlock() const {
        return _iterCheck.doesBlock();
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
    const IterationCompletionCheck _iterCheck;
    std::unique_ptr<T> _value;
    const PhaseNumber _inPhase;

};  // class ActorPhase


template <class T>
using PhaseMap = std::unordered_map<PhaseNumber, V1::ActorPhase<T>>;


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

        // TODO: we can detect this at setup time - worth doing now?
        if (found == _phaseMap.end()) {
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
 * @attention Only use this in range-based for loops.
 *
 * Iterates over all phases and will correctly call
 * `awaitPhaseStart()` and `awaitPhaseEnd()` in the
 * correct operators.
 *
 * ```c++
 * class MyActor : Actor {
 *   // TODO: update example
 *   void run() override {
 *     for(auto&& phase : orchestrator.loop(blocking))
 *       while(phase == orchestrator.currentPhase())
 *         doOperation(phase);
 *   }
 * }
 * ```
 *
 * This should **only** be used by range-based for loops because
 * the implementation relies on callers alternating between
 * `operator*()` and `operator++()` to indicate the caller's
 * done-ness or readiness of the current/next phase.
 *
 * TODO: incorporate into description of how Phases blocks are read:
 *
 *      Non-blocking means that the iterator will immediately call
 *      awaitPhaseEnd() right after calling awaitPhaseStart(). This
 *      will prevent the Orchestrator from waiting for this Actor
 *      to complete its operations in the current Phase.
 *
 *      Note that the Actor still needs to wait for the next Phase
 *      to start before going on to the next iteration of the loop.
 *      The common way to do this is to periodically check that
 *      the current Phase number (`Orchestrator::currentPhase()`)
 *      hasn't changed.
 *
 *      The `PhaseLoop` type will soon be incorporated into this type
 *      and will support automatically doing this check if required.
 *
 */
template <class T>
class PhaseLoop final {

public:
    explicit PhaseLoop(ActorContext& context)
        : PhaseLoop(context.orchestrator(), std::move(constructPhaseMap(context))) {}

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
    static V1::PhaseMap<T> constructPhaseMap(ActorContext& actorContext) {

        // clang-format off
        static_assert(std::is_constructible_v<T, PhaseContext&>);
        static_assert(std::is_constructible_v<V1::ActorPhase<T>, Orchestrator&, PhaseContext&, PhaseNumber, PhaseContext&>);
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
                // last arg gets forwarded to T ctor (via forward inside of make_unique)
                *phaseContext);
        }
        return out;
    }

    Orchestrator& _orchestrator;
    V1::PhaseMap<T> _phaseMap;  // we own it
    // _phaseMap cannot be const since we don't want to enforce that the wrapped u_p<T> is const

};  // class PhaseLoop


}  // namespace genny

#endif  // HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
