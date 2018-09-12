#ifndef HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
#define HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED

#include <cassert>
#include <chrono>
#include <iterator>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>

/**
 * General TODO:
 * - see what can be pushed into a cpp
 * - see what ctors can be hidden or private
 * - fix remaining orchestrator_test tests
 * - static_assert on the T value that it has to have the right ctor?
 * - try to make the T ctor only take a PhaseContext ref
 * - doc everything (like it's hot).
 * - Doc interactions with this in context.hpp
 * - maybe track Iters and StartedAt in IterationCompletionCheck?
 * - move tests from Orchestrator test into PhaseLoop tests
 * - Kill PhasedActor
 */

/*
 * Reminder: the V1 namespace types are *not* intended to be used directly.
 */
namespace genny::V1 {

class IterationCompletionCheck {

public:
    explicit IterationCompletionCheck() : IterationCompletionCheck(std::nullopt, std::nullopt) {}

    IterationCompletionCheck(std::optional<int> _minIterations,
                             std::optional<std::chrono::milliseconds> _minDuration)
        : _minDuration(_minDuration), _minIterations(_minIterations) {

        if (_minIterations && *_minIterations < 0) {
            std::stringstream str;
            str << "Need non-negative number of iterations. Gave " << *_minIterations;
            throw InvalidConfigurationException(str.str());
        }
        if (_minDuration && _minDuration->count() < 0) {
            std::stringstream str;
            str << "Need non-negative duration. Gave " << _minDuration->count() << " milliseconds";
            throw InvalidConfigurationException(str.str());
        }
    }

    explicit IterationCompletionCheck(const std::unique_ptr<PhaseContext>& phaseContext)
        : IterationCompletionCheck(
              phaseContext->get<int, false>("Repeat"),
              phaseContext->get<std::chrono::milliseconds, false>("Duration")) {}

    std::chrono::steady_clock::time_point startedAt() const {
        return _minDuration ? std::chrono::steady_clock::now()
                            : std::chrono::time_point<std::chrono::steady_clock>::min();
    }

    bool isDone(unsigned int currentIteration,
                std::chrono::steady_clock::time_point startedAt) const {
        return doneIterations(currentIteration) && doneDuration(startedAt);
    }

    bool operator==(const IterationCompletionCheck& other) const {
        return _minDuration == other._minDuration && _minIterations == other._minIterations;
    }

    bool doesBlock() const {
        return _minIterations || _minDuration;
    }

private:
    bool doneIterations(unsigned int currentIteration) const {
        return !_minIterations || currentIteration >= *_minIterations;
    }

    bool doneDuration(std::chrono::steady_clock::time_point startedAt) const {
        return !_minDuration ||
            // check is last in chain to avoid doing now() call unnecessarily
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                  startedAt) >= *_minDuration;
    }

    const std::optional<std::chrono::milliseconds> _minDuration;
    const std::optional<int> _minIterations;
};

/**
 * Configured with {@link IterationCompletionCheck} and will continue
 * iterating until configured #iterations or duration are done
 * or, if non-blocking, when Orchestrator says phase has changed.
 */
class ActorPhaseIterator {

public:
    // iterator concept value-type
    // intentionally empty; most compilers will elide any actual storage
    struct Value {};

    explicit ActorPhaseIterator(Orchestrator& orchestrator,
                                bool isEnd,
                                const IterationCompletionCheck& iterCheck)
        : _isEndIterator{isEnd},
          _currentIteration{0},
          _orchestrator{orchestrator},
          _iterCheck{iterCheck},
          _startedAt{_iterCheck.startedAt()} {}

    explicit ActorPhaseIterator(Orchestrator& orchestrator, bool isEnd)
        : ActorPhaseIterator{orchestrator, isEnd, IterationCompletionCheck{}} {}

    Value operator*() const {
        return Value();
    }

    ActorPhaseIterator& operator++() {
        ++_currentIteration;
        return *this;
    }

    // clang-format off
    bool operator==(const ActorPhaseIterator& rhs) const {
        // TODO: check orchestrator if non-blocking
        return
                (rhs._isEndIterator && _iterCheck.isDone(_currentIteration, _startedAt))

                // Below checks are mostly for pure correctness;
                //   "well-formed" code will only use this iterator in range-based for-loops and will thus
                //   never use these conditions.

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
    const bool _isEndIterator;
    const IterationCompletionCheck& _iterCheck;

    const std::chrono::steady_clock::time_point _startedAt;
    Orchestrator& _orchestrator;
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

template <class T>
class ActorPhase {

public:
    // can't copy anyway due to unique_ptr but may as well be explicit (may make error messages
    // better)
    ActorPhase(const ActorPhase&) = delete;
    void operator=(const ActorPhase&) = delete;

    ActorPhase(Orchestrator &orchestrator, IterationCompletionCheck iterCheck, std::unique_ptr<T> &&value)
        : _orchestrator(orchestrator),
          _value(std::move(value)),
          _iterCheck(std::move(iterCheck)) {}

    // TODO: forward args to make_unique
    ActorPhase(Orchestrator& orchestrator,
               const std::unique_ptr<PhaseContext>& phaseContext,
               std::unique_ptr<T>&& value)
        : ActorPhase(orchestrator, IterationCompletionCheck{phaseContext}, std::move(value)) {}

    ActorPhaseIterator begin() {
        return ActorPhaseIterator{_orchestrator, false, _iterCheck};
    }

    ActorPhaseIterator end() {
        return ActorPhaseIterator{_orchestrator, true};
    };

    bool doesBlock() const {
        return _iterCheck.doesBlock();
    }

    // TODO: don't expose unique_ptr() here; have our own strict return type
    auto operator-> () const {
        return _value.operator->();
    }

    // TODO: don't expose unique_ptr() here; have our own strict return type
    auto operator*() const {
        return _value.operator*();
    }

private:
    Orchestrator& _orchestrator;
    std::unique_ptr<T> _value;

    const IterationCompletionCheck _iterCheck;

};  // class ActorPhase


template <class T>
class PhaseLoopIterator {

public:
    // These are intentionally commented-out because this type
    // should not be used by any    std algorithms that may rely on them.
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

    bool operator!=(const PhaseLoopIterator& other) const {
        // Intentionally don't handle self-equality or other "normal" cases.
        return !(other._isEnd && !this->morePhases());
    }

    // intentionally non-const
    std::pair<PhaseNumber, ActorPhase<T>&> operator*() {
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

    // TODO: ensure consistent ordering per CONTRIBUTING.md
    explicit PhaseLoopIterator(Orchestrator& orchestrator,
                               std::unordered_map<PhaseNumber, ActorPhase<T>>& phaseMap,
                               bool isEnd)
        : _orchestrator{orchestrator},
          _phaseMap{phaseMap},
          _isEnd{isEnd},
          _currentPhase{0},
          _awaitingPlusPlus{false} {}

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
    std::unordered_map<PhaseNumber, ActorPhase<T>>& _phaseMap;  // cannot be const

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

};  // class PhaseLoopIterator


}  // namespace genny::V1

// TODO: nest namespaces rather than starting separate block
namespace genny {

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
class PhaseLoop {

    using PhaseMap = std::unordered_map<PhaseNumber, V1::ActorPhase<T>>;

public:
    V1::PhaseLoopIterator<T> begin() {
        return V1::PhaseLoopIterator<T>{this->_orchestrator, this->_phaseMap, false};
    }

    V1::PhaseLoopIterator<T> end() {
        return V1::PhaseLoopIterator<T>{this->_orchestrator, this->_phaseMap, true};
    }

    // TODO: should this be private?
    PhaseLoop(Orchestrator& orchestrator, PhaseMap phaseMap)
        : _orchestrator{orchestrator}, _phaseMap{std::move(phaseMap)} {
        // propagate this Actor's set up PhaseNumbers to Orchestrator
        for (auto&& [phaseNum, actorPhase] : _phaseMap) {
            orchestrator.phasesAtLeastTo(phaseNum);
        }
    }

    PhaseLoop(genny::ActorContext& context)
        : PhaseLoop(context.orchestrator(), constructPhaseMap(context)) {}

private:
    static PhaseMap constructPhaseMap(ActorContext& actorContext) {
        PhaseMap out;
        for (auto&& [num, phaseContext] : actorContext.phases()) {
            out.try_emplace(
                // key, (args-to-value-ctor => args-to-ActorPhase<T> ctor)
                num,
                actorContext.orchestrator(),
                phaseContext,
                std::make_unique<T>(phaseContext));
        }
        return out;
    }

    Orchestrator& _orchestrator;
    PhaseMap _phaseMap;  // we own it
    // _PhaseMap cannot be const since we don't want to enforce that the wrapped u_p<T> is const

};  // class PhaseLoop


}  // namespace genny

#endif  // HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
