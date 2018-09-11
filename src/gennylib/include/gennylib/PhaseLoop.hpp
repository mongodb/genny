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


/*
 * Reminder: the V1 namespace types are *not* intended to be used directly.
 */
namespace genny::V1 {

class ItersAndDuration {

public:
    explicit ItersAndDuration() : ItersAndDuration(std::nullopt, std::nullopt) {}

    ItersAndDuration(std::optional<int> _minIterations,
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

    ItersAndDuration(const std::unique_ptr<PhaseContext>& phaseContext)
        : ItersAndDuration(phaseContext->get<int, false>("Repeat"),
                           phaseContext->get<std::chrono::milliseconds, false>("Duration")) {}

    std::chrono::steady_clock::time_point startedAt() const {
        return _minDuration ? std::chrono::steady_clock::now()
                            : std::chrono::time_point<std::chrono::steady_clock>::min();
    }

    bool doneIterations(int currentIteration) const {
        return !_minIterations || currentIteration >= *_minIterations;
    }

    bool doneDuration(std::chrono::steady_clock::time_point startedAt) const {
        return !_minDuration ||
            // check is last in chain to avoid doing now() call unnecessarily
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                  startedAt) >= *_minDuration;
    }

    bool operator==(const ItersAndDuration& other) const {
        return _minDuration == other._minDuration && _minIterations == other._minIterations;
    }

    bool doesBlock() const {
        return _minIterations || _minDuration;
    }

private:
    const std::optional<std::chrono::milliseconds> _minDuration;
    const std::optional<int> _minIterations;
};

/**
 * Tracks the iteration-state of a `OperationLoop`.
 */
// This is intentionally header-only to help avoid doing unnecessary function-calls.
class OperationLoopIterator {

public:
    // <iterator-concept>
    struct Value {};  // intentionally empty; most compilers will elide any actual storage
    typedef std::forward_iterator_tag iterator_category;
    typedef Value value_type;
    typedef Value reference;
    typedef Value pointer;
    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

    explicit OperationLoopIterator(Orchestrator& orchestrator,
                                   bool isEnd,
                                   ItersAndDuration itersAndDuration)
        : _isEndIterator{isEnd},
          _currentIteration{0},
          _orchestrator{orchestrator},
          _itersAndDuration{itersAndDuration},
          _startedAt{_itersAndDuration.startedAt()} {}

    explicit OperationLoopIterator(Orchestrator& orchestrator, bool isEnd)
        : OperationLoopIterator{orchestrator, isEnd, ItersAndDuration{}} {}

    Value operator*() const {
        return Value();
    }

    OperationLoopIterator& operator++() {
        ++_currentIteration;
        return *this;
    }

    // clang-format off
    bool operator==(const OperationLoopIterator& rhs) const {
        // I heard you like terse, short-circuiting business-logic, bro, so I wrote you a love-letter to || ❤️
        return
            // Comparing this == .end(). This is most common call in range-based for-loops, so do it first.
                (rhs._isEndIterator
                 // Need to see if this is in end state. There are two conditions
                 // 1. minIterations is empty-optional or we're at or past minIterations
                 && (_itersAndDuration.doneIterations(_currentIteration))
                 // 2. minDuration is empty-optional or we've exceeded minDuration
                 && (_itersAndDuration.doneDuration(_startedAt)))

                // Below checks are mostly for pure correctness;
                //   "well-formed" code will only use this iterator in range-based for-loops and will thus
                //   never use these conditions.

                // this == this
                || (this == &rhs)

                // neither is end iterator but have same fields
                || (!rhs._isEndIterator && !_isEndIterator
                    && _startedAt        == rhs._startedAt
                    && _currentIteration == rhs._currentIteration
                    && _itersAndDuration == rhs._itersAndDuration)

                // both .end() iterators (all .end() iterators are ==)
                || (_isEndIterator && rhs._isEndIterator)

                // we're .end(), so 'recurse' but flip the args so 'this' is rhs
                || (_isEndIterator && rhs == *this);
    }
    // clang-format on

    // Iterator concepts only require !=, but the logic is much easier to reason about
    // for ==, so just negate that logic 😎 (compiler should inline it)
    bool operator!=(const OperationLoopIterator& rhs) const {
        return !(*this == rhs);
    }

private:
    const bool _isEndIterator;
    const ItersAndDuration _itersAndDuration;

    std::chrono::steady_clock::time_point _startedAt;
    Orchestrator& _orchestrator;
    unsigned int _currentIteration;
};

template <class T>
class ActorPhase {

public:
    // can't copy anyway due to unique_ptr but may as well be explicit (may make error messages
    // better)
    ActorPhase(const ActorPhase&) = delete;
    void operator=(const ActorPhase&) = delete;

    ActorPhase(ActorPhase&& other) noexcept
        : _orchestrator{other._orchestrator},
          _value{std::move(other._value)},
          _itersAndDuration{std::move(other._itersAndDuration)} {}

    ActorPhase(Orchestrator& _orchestrator,
               std::unique_ptr<T>&& _value,
               ItersAndDuration itersAndDuration)
        : _orchestrator(_orchestrator),
          _value(std::move(_value)),
          _itersAndDuration(itersAndDuration) {}

    ActorPhase(Orchestrator& orchestrator,
               const std::unique_ptr<PhaseContext>& phaseContext,
               std::unique_ptr<T>&& value)
        : ActorPhase(orchestrator, std::move(value), ItersAndDuration{phaseContext}) {}

    OperationLoopIterator begin() {
        return OperationLoopIterator{_orchestrator, false, _itersAndDuration};
    }

    OperationLoopIterator end() {
        return OperationLoopIterator{_orchestrator, true};
    };

    bool doesBlock() const {
        return _itersAndDuration.doesBlock();
    }

    auto operator-> () const {
        return _value.operator->();
    }

    auto operator*() const {
        return _value.operator*();
    }

private:
    Orchestrator& _orchestrator;
    std::unique_ptr<T> _value;

    const ItersAndDuration _itersAndDuration;
};

// Only usable in range-based for loops.
template <class T>
class PhaseLoopIterator {

public:
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

    bool operator!=(const PhaseLoopIterator& other) const {
        // Intentionally don't handle self-equality or other "normal" cases.
        //
        // This type is only intended to be used by range-based for-loops
        // and their equivalent expanded definitions
        // https://en.cppreference.com/w/cpp/language/range-for
        return !(other._isEnd && !this->morePhases());
    }

    // intentionally non-const
    std::pair<PhaseNumber, ActorPhase<T>&> operator*() {
        assert(!_awaitingPlusPlus);

        // Intentionally don't bother with cases where user didn't call operator++()
        // between invocations of operator*() and vice-versa.
        //
        //
        // This type is only intended to be used by range-based for-loops
        // and their equivalent expanded definitions
        // https://en.cppreference.com/w/cpp/language/range-for
        _currentPhase = this->_orchestrator->awaitPhaseStart();
        if (!this->doesBlockOn(_currentPhase)) {
            this->_orchestrator->awaitPhaseEnd(false);
        }

        _awaitingPlusPlus = true;

        auto&& found = _phaseMap.find(_currentPhase);

        // XXX: we can detect this at setup time
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
        //
        // This type is only intended to be used by range-based for-loops
        // and their equivalent expanded definitions
        // https://en.cppreference.com/w/cpp/language/range-for
        if (this->doesBlockOn(_currentPhase)) {
            this->_orchestrator->awaitPhaseEnd(true);
        }

        _awaitingPlusPlus = false;
        return *this;
    }

    explicit PhaseLoopIterator(Orchestrator* orchestrator,
                               std::unordered_map<PhaseNumber, ActorPhase<T>>& holders,
                               bool isEnd)
        : _orchestrator{orchestrator},
          _phaseMap{holders},
          _isEnd{isEnd},
          _currentPhase{0},
          _awaitingPlusPlus{false} {}

private:
    bool morePhases() const {
        return this->_orchestrator->morePhases();
    }

    bool doesBlockOn(PhaseNumber phase) const {
        if (auto h = _phaseMap.find(phase); h != _phaseMap.end()) {
            return h->second.doesBlock();
        }
        return true;
    }

    Orchestrator* _orchestrator;
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
};

/**
 * Configured with an optional<min#iterations> and/or optional<min duration>. The
 * returned .begin() iterators will not == .end() until both the # iterations and
 * duration requirements are met.
 *
 * Can be used as-is but intended to be used from `context.hpp` classes and
 * configured from conventions.
 *
 * See extended example in `PhaseContext.loop()`.
 */
class OperationLoop {

public:
    template <class... Args>
    explicit OperationLoop(Orchestrator& orchestrator, Args&&... args)
        : _orchestrator{orchestrator}, _itersAndDuration{std::forward<Args>(args)...} {}


    V1::OperationLoopIterator begin() {
        return V1::OperationLoopIterator{_orchestrator, false, _itersAndDuration};
    }

    V1::OperationLoopIterator end() {
        return V1::OperationLoopIterator{_orchestrator, true};
    }

private:
    Orchestrator& _orchestrator;
    const ItersAndDuration _itersAndDuration;
};

}  // namespace genny::V1

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

    PhaseLoop(Orchestrator& orchestrator, PhaseMap&& holders)
        : _orchestrator{std::addressof(orchestrator)}, _phaseMap{std::move(holders)} {
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
            auto&& deref = phaseContext;
            out.try_emplace(num, actorContext.orchestrator(), deref, std::make_unique<T>(deref));
        }
        return out;
    }

    Orchestrator* const _orchestrator;
    PhaseMap _phaseMap;
};

}  // namespace genny

#endif  // HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
