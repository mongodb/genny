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

#ifndef HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
#define HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED

#include <cassert>
#include <chrono>
#include <iterator>
#include <optional>
#include <sstream>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <boost/exception/exception.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/v1/GlobalRateLimiter.hpp>
#include <gennylib/v1/Sleeper.hpp>

/**
 * @file
 * This file provides the `PhaseLoop<T>` type and the collaborator classes that make it iterable.
 * See extended example on the PhaseLoop class docs.
 */

namespace genny {

/*
 * Reminder: the v1 namespace types are *not* intended to be used directly.
 */
namespace v1 {

using SteadyClock = std::chrono::steady_clock;

/**
 * Determine the conditions for continuing to iterate a given Phase.
 *
 * One of these is constructed for each `ActorPhase<T>` (below)
 * using a PhaseContext's `Repeat` and `Duration` keys. It is
 * then passed to the downstream `ActorPhaseIterator` which
 * actually keeps track of the current state of the iteration in
 * `for(auto _ : phase)` loops. The `ActorPhaseIterator` keeps track
 * of how many iterations have been completed and, if necessary,
 * when the iterations started. These two values (# iterations and
 * iteration start time) are passed into the IterationCompletionCheck
 * to determine if the loop should continue iterating.
 */
class IterationChecker final {

public:
    IterationChecker(std::optional<TimeSpec> minDuration,
                     std::optional<IntegerSpec> minIterations,
                     bool isNop,
                     TimeSpec sleepBefore,
                     TimeSpec sleepAfter,
                     std::optional<RateSpec> rateSpec)
        : _minDuration{minDuration},
          // If it is a nop then should iterate 0 times.
          _minIterations{isNop ? IntegerSpec(0l) : minIterations},
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

        if ((sleepBefore || sleepAfter) && rateSpec) {
            throw InvalidConfigurationException(
                "GlobalRate must *not* be specified alongside either sleepBefore or sleepAfter. "
                "genny cannot enforce the global rate when there are mandatory sleeps in"
                "each thread");
        }

        _sleeper.emplace(sleepBefore, sleepAfter);
    }

    explicit IterationChecker(PhaseContext& phaseContext)
        : IterationChecker(phaseContext["Duration"].maybe<TimeSpec>(),
                           phaseContext["Repeat"].maybe<IntegerSpec>(),
                           phaseContext.isNop(),
                           phaseContext["SleepBefore"].maybe<TimeSpec>().value_or(TimeSpec{}),
                           phaseContext["SleepAfter"].maybe<TimeSpec>().value_or(TimeSpec{}),
                           phaseContext["GlobalRate"].maybe<RateSpec>()) {
        if (!phaseContext.isNop() && !phaseContext["Duration"] && !phaseContext["Repeat"] &&
            phaseContext["Blocking"].maybe<std::string>() != "None") {
            std::stringstream msg;
            msg << "Must specify 'Blocking: None' for Actors in Phases that don't block "
                   "completion with a Repeat or Duration value. In Phase "
                << phaseContext.path() << ". Gave";
            msg << " Duration:"
                << phaseContext["Duration"].maybe<std::string>().value_or("undefined");
            msg << " Repeat:" << phaseContext["Repeat"].maybe<std::string>().value_or("undefined");
            msg << " Blocking:"
                << phaseContext["Blocking"].maybe<std::string>().value_or("undefined");
            throw InvalidConfigurationException(msg.str());
        }

        const auto rateSpec = phaseContext["GlobalRate"].maybe<RateSpec>();
        const auto rateLimiterName =
            phaseContext["RateLimiterName"].maybe<std::string>().value_or("defaultRateLimiter");

        if (rateSpec) {
            std::ostringstream defaultRLName;
            defaultRLName << phaseContext.actor()["Name"] << phaseContext.getPhaseNumber();
            const auto rateLimiterName =
                phaseContext["RateLimiterName"].maybe<std::string>().value_or(defaultRLName.str());

            if (!_doesBlock) {
                throw InvalidConfigurationException(
                    "GlobalRate must be specified alongside either Duration or Repeat, otherwise "
                    "there's no guarantee the rate limited operation will run in the correct "
                    "phase");
            }
            _rateLimiter =
                phaseContext.workload().getRateLimiter(rateLimiterName, rateSpec.value());
        }
    }

    constexpr void limitRate(const SteadyClock::time_point referenceStartingPoint,
                             const int64_t currentIteration,
                             const PhaseNumber inPhase) {
        // This function is called after each iteration, so we never rate limit the
        // first iteration. This means the number of completed operations is always
        // `n * GlobalRateLimiter::_burstSize + m` instead of an exact multiple of
        // _burstSize. `m` here is the number of threads using the rate limiter.
        if (_rateLimiter) {
            while (true) {
                const auto now = SteadyClock::now();
                auto success = _rateLimiter->consumeIfWithinRate(now);
                if (!success && !isDone(referenceStartingPoint, currentIteration, now)) {

                    // Don't sleep for more than 1 second (1e9 nanoseconds). Otherwise rates
                    // specified in seconds or lower resolution can cause the workloads to
                    // run visibly longer than the specified duration.
                    const auto rate = _rateLimiter->getRate() > 1e9 ? 1e9 : _rateLimiter->getRate();

                    // Add ±5% jitter to avoid threads waking up at once.
                    std::this_thread::sleep_for(std::chrono::nanoseconds(
                        int64_t(rate * (0.95 + 0.1 * (double(rand()) / RAND_MAX)))));
                    continue;
                }
                break;
            }
            _rateLimiter->notifyOfIteration();
        }
    }

    constexpr SteadyClock::time_point computeReferenceStartingPoint() const {
        // avoid doing now() if no minDuration configured
        return _minDuration ? SteadyClock::now() : SteadyClock::time_point::min();
    }

    constexpr bool isDone(SteadyClock::time_point startedAt,
                          int64_t currentIteration,
                          SteadyClock::time_point now) {
        return (!_minIterations || currentIteration >= (*_minIterations).value) &&
            (!_minDuration || (*_minDuration).value <= now - startedAt);
    }

    constexpr bool operator==(const IterationChecker& other) const {
        return _minDuration == other._minDuration && _minIterations == other._minIterations;
    }

    constexpr bool doesBlockCompletion() const {
        return _doesBlock;
    }

    constexpr void sleepBefore(const Orchestrator& o, const PhaseNumber pn) const {
        _sleeper->before(o, pn);
    }

    constexpr void sleepAfter(const Orchestrator& o, const PhaseNumber pn) const {
        _sleeper->after(o, pn);
    }

private:
    // Debatable about whether this should also track the current iteration and
    // referenceStartingPoint time (versus having those in the ActorPhaseIterator). BUT: even the
    // .end() iterator needs an instance of this, so it's weird

    const std::optional<TimeSpec> _minDuration;
    const std::optional<IntegerSpec> _minIterations;

    // The rate limiter is owned by the workload context.
    v1::GlobalRateLimiter* _rateLimiter = nullptr;
    const bool _doesBlock;  // Computed/cached value. Computed at ctor time.
    std::optional<v1::Sleeper> _sleeper;
};


/**
 * The iterator used in `for(auto _ : cfg)` and returned from
 * `ActorPhase::begin()` and `ActorPhase::end()`.
 *
 * Configured with {@link IterationCompletionCheck} and will continue
 * iterating until configured #iterations or duration are done
 * or, if non-blocking, when Orchestrator says phase has changed.
 */
class ActorPhaseIterator final {

public:
    // Normally we'd use const IterationChecker& (ref rather than *)
    // but we actually *want* nullptr for the end iterator. This is a bit of
    // an over-optimization, but it adds very little complexity at the benefit
    // of not having to construct a useless object.
    ActorPhaseIterator(Orchestrator& orchestrator,
                       IterationChecker* iterationCheck,
                       PhaseNumber inPhase,
                       bool isEndIterator)
        : _orchestrator{std::addressof(orchestrator)},
          _iterationCheck{iterationCheck},
          _referenceStartingPoint{isEndIterator ? SteadyClock::time_point::min()
                                                : _iterationCheck->computeReferenceStartingPoint()},
          _inPhase{inPhase},
          _isEndIterator{isEndIterator},
          _currentIteration{0} {
        // iterationCheck should only be null if we're end() iterator.
        assert(isEndIterator == (iterationCheck == nullptr));
    }

    // iterator concept value-type
    // intentionally empty; most compilers will elide any actual storage
    struct Value {};

    Value operator*() const {
        return Value();
    }

    constexpr ActorPhaseIterator& operator++() {
        if (_iterationCheck) {
            _iterationCheck->sleepAfter(*_orchestrator, _inPhase);
        }
        ++_currentIteration;
        return *this;
    }

    bool operator==(const ActorPhaseIterator& rhs) const {
        if (_iterationCheck) {
            _iterationCheck->sleepBefore(*_orchestrator, _inPhase);
            _iterationCheck->limitRate(_referenceStartingPoint, _currentIteration, _inPhase);
        }
        // clang-format off
        return
                // we're comparing against the .end() iterator (the common case)
                (rhs._isEndIterator && !this->_isEndIterator &&
                   (!_orchestrator->continueRunning() ||
                     // ↑ orchestrator says we stop
                     // ...or...
                     // if we block, then check to see if we're done in current phase
                     // else check to see if current phase has expired
                     (_iterationCheck->doesBlockCompletion()
                            ? _iterationCheck->isDone(_referenceStartingPoint, _currentIteration, SteadyClock::now())
                            : _orchestrator->currentPhase() != _inPhase)))

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
                    && _iterationCheck          == rhs._iterationCheck)

                // both .end() iterators (all .end() iterators are ==)
                || (_isEndIterator && rhs._isEndIterator)

                // we're .end(), so 'recurse' but flip the args so 'this' is rhs
                || (_isEndIterator && rhs == *this);
    }
    // clang-format on

    // Iterator concepts only require !=, but the logic is much easier to reason about
    // for ==, so just negate that logic 😎 (compiler should inline it)
    const bool operator!=(const ActorPhaseIterator& rhs) const {
        return !(*this == rhs);
    }

private:
    Orchestrator* _orchestrator;
    IterationChecker* _iterationCheck;
    const SteadyClock::time_point _referenceStartingPoint;
    const PhaseNumber _inPhase;
    const bool _isEndIterator;
    int64_t _currentIteration;

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
 * This type can be used as a `T*` through its implicit conversion
 * and by its operator-overloads.
 *
 * This is intended to be used via `PhaseLoop` below.
 */
template <class T>
class ActorPhase final {

public:
    /**
     * `args` are forwarded as the T value's constructor-args
     */
    template <class... Args>
    ActorPhase(Orchestrator& orchestrator,
               std::unique_ptr<IterationChecker> iterationCheck,
               PhaseNumber currentPhase,
               Args&&... args)
        : _orchestrator{orchestrator},
          _currentPhase{currentPhase},
          _value{std::make_unique<T>(std::forward<Args>(args)...)},
          _iterationCheck{std::move(iterationCheck)} {
        static_assert(std::is_constructible_v<T, Args...>);
    }

    /**
     * `args` are forwarded as the T value's constructor-args
     */
    template <class... Args>
    ActorPhase(Orchestrator& orchestrator,
               PhaseContext& phaseContext,
               PhaseNumber currentPhase,
               Args&&... args)
        : _orchestrator{orchestrator},
          _currentPhase{currentPhase},
          _value{!phaseContext.isNop() ? std::make_unique<T>(std::forward<Args>(args)...)
                                       : nullptr},
          _iterationCheck{std::make_unique<IterationChecker>(phaseContext)} {
        static_assert(std::is_constructible_v<T, Args...>);
    }

    ActorPhaseIterator begin() {
        return ActorPhaseIterator{_orchestrator, _iterationCheck.get(), _currentPhase, false};
    }

    ActorPhaseIterator end() {
        return ActorPhaseIterator{_orchestrator, nullptr, _currentPhase, true};
    };

    // Used by PhaseLoopIterator::doesBlockCompletion()
    constexpr bool doesBlock() const {
        return _iterationCheck->doesBlockCompletion();
    }

    // Checks if the actor is performing a nullOp. Used only for testing.
    constexpr bool isNop() const {
        return !_value;
    }

    // Could use `auto` for return-type of operator-> and operator*, but
    // IDE auto-completion likes it more if it's spelled out.
    //
    // - `remove_reference_t` is to handle the case when it's `T&`
    //   (which it theoretically can be in deduced contexts).
    // - `remove_reference_t` is idempotent so it's also just defensive-programming
    // - `add_pointer_t` ensures it's a pointer :)
    //
    // BUT: this is just duplicated from the signature of `std::unique_ptr<T>::operator->()`
    //      so we trust the STL to do the right thing™️
    typename std::add_pointer_t<std::remove_reference_t<T>> operator->() const noexcept {
#ifndef NDEBUG
        if (!_value) {
            BOOST_THROW_EXCEPTION(std::logic_error("Trying to dereference via -> in a Nop phase."));
        }
#endif
        return _value.operator->();
    }

    // Could use `auto` for return-type of operator-> and operator*, but
    // IDE auto-completion likes it more if it's spelled out.
    //
    // - `add_lvalue_reference_t` to ensure that we indicate we're a ref not a value
    //
    // BUT: this is just duplicated from the signature of `std::unique_ptr<T>::operator*()`
    //      so we trust the STL to do the right thing™️
    typename std::add_lvalue_reference_t<T> operator*() const {
#ifndef NDEBUG
        if (!_value) {
            BOOST_THROW_EXCEPTION(std::logic_error("Trying to dereference via * in a Nop phase."));
        }
#endif
        return _value.operator*();
    }

    // Allow ActorPhase<T> to be used as a T* through implicit conversion.
    operator std::add_pointer_t<std::remove_reference_t<T>>() {
#ifndef NDEBUG
        if (!_value) {
            BOOST_THROW_EXCEPTION(std::logic_error("Trying to dereference via * in a Nop phase."));
        }
#endif
        return _value.operator->();
    }

    PhaseNumber phaseNumber() const {
        return _currentPhase;
    }

private:
    Orchestrator& _orchestrator;
    const PhaseNumber _currentPhase;
    const std::unique_ptr<T> _value;  // nullptr iff operation is Nop
    const std::unique_ptr<IterationChecker> _iterationCheck;

};  // class ActorPhase


/**
 * Maps from PhaseNumber to the ActorPhase<T> to be used in that PhaseNumber.
 */
template <class T>
using PhaseMap = std::unordered_map<PhaseNumber, v1::ActorPhase<T>>;


/**
 * The iterator used by `for(auto&& config : phaseLoop)`.
 *
 * @attention Don't use this outside of range-based for loops.
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

    ActorPhase<T>& operator*() /* cannot be const */ {
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
        return found->second;
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

    constexpr bool operator!=(const PhaseLoopIterator& other) const {
        // Intentionally don't handle self-equality or other "normal" cases.
        return !(other._isEnd && !this->morePhases());
    }

private:
    constexpr bool morePhases() const {
        return this->_orchestrator.morePhases();
    }

    constexpr bool doesBlockOn(PhaseNumber phase) const {
        if (auto item = _phaseMap.find(phase); item != _phaseMap.end()) {
            return item->second.doesBlock();
        }
        return true;
    }

    Orchestrator& _orchestrator;
    PhaseMap<T>& _phaseMap;  // cannot be const; owned by PhaseLoop

    const bool _isEnd;

    // can't just always look this up from the Orchestrator. When we're
    // doing operator++() we need to know what the value of the phase
    // was during operator*() so we can check if it was blocking or not.
    // If we don't store the value during operator*() the Phase value
    // may have changed already.
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

}  // namespace v1


/**
 * @return an object that iterates over all configured Phases, calling `awaitPhaseStart()`
 *         and `awaitPhaseEnd()` at the appropriate times. The value-type, `ActorPhase`,
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
 *         // to do for each Phase. Likely holds Expressions or other
 *         // expensive-to-construct objects. PhaseLoop will construct these
 *         // at Actor setup time rather than at runtime.
 *         struct MyActorConfig {
 *             int _myImportantThing;
 *             // Must have a ctor that takes a PhaseContext& as first arg.
 *             // Other ctor args are forwarded from PhaseLoop ctor.
 *             MyActorConfig(PhaseContext& phaseConfig)
 *             : _myImportantThing{phaseConfig.get<int>("ImportantThing")} {}
 *         };
 *
 *         PhaseLoop<MyActorConfig> _loop;
 *
 *     public:
 *         MyActor(ActorContext& actorContext)
 *         : _loop{actorContext} {}
 *         // if your MyActorConfig takes other ctor args, pass them through
 *         // here e.g. _loop{actorContext, someOtherParam}
 *
 *         void run() {
 *             for(auto&& [phaseNum, actorPhase] : _loop) {     // (1)
 *                 // Access the MyActorConfig for the Phase
 *                 // by using operator->() or operator*().
 *                 auto importantThingForThisPhase = actorPhase->_myImportantThing;
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
                    std::move(constructPhaseMap(context, std::forward<Args>(args)...))) {
        // Some of these static_assert() calls are redundant. This is to help
        // users more easily track down compiler errors.
        //
        // Don't do this at the class level because tests want to be able to
        // construct a simple PhaseLoop<int>.
        static_assert(std::is_constructible_v<T, PhaseContext&, Args...>);
    }

    // Only visible for testing
    PhaseLoop(Orchestrator& orchestrator, v1::PhaseMap<T> phaseMap)
        : _orchestrator{orchestrator}, _phaseMap{std::move(phaseMap)} {
        // propagate this Actor's set up PhaseNumbers to Orchestrator
    }

    v1::PhaseLoopIterator<T> begin() {
        return v1::PhaseLoopIterator<T>{this->_orchestrator, this->_phaseMap, false};
    }

    v1::PhaseLoopIterator<T> end() {
        return v1::PhaseLoopIterator<T>{this->_orchestrator, this->_phaseMap, true};
    }

private:
    template <class... Args>
    static v1::PhaseMap<T> constructPhaseMap(ActorContext& actorContext, Args&&... args) {

        // clang-format off
        static_assert(std::is_constructible_v<T, PhaseContext&, Args...>);
        // kinda redundant with ↑ but may help error-handling
        static_assert(std::is_constructible_v<v1::ActorPhase<T>, Orchestrator&, PhaseContext&, PhaseNumber, PhaseContext&, Args...>);
        // clang-format on

        v1::PhaseMap<T> out;
        for (auto&& [num, phaseContext] : actorContext.phases()) {
            auto [it, success] = out.try_emplace(
                // key
                num,
                // args to ActorPhase<T> ctor:
                actorContext.orchestrator(),
                *phaseContext,
                num,
                // last arg(s) get forwarded to T ctor (via forward inside of make_unique)
                *phaseContext,
                std::forward<Args>(args)...);
            if (!success) {
                // This should never happen because genny::ActorContext::constructPhaseContexts
                // ensures we can't configure duplicate Phases.
                std::stringstream msg;
                msg << "Duplicate phase " << num;
                throw InvalidConfigurationException(msg.str());
            }
        }
        return out;
    }

    Orchestrator& _orchestrator;
    v1::PhaseMap<T> _phaseMap;
    // _phaseMap cannot be const since we don't want to enforce
    // the wrapped unique_ptr<T> in ActorPhase<T> to be const.

};  // class PhaseLoop


}  // namespace genny

#endif  // HEADER_10276107_F885_4F2C_B99B_014AF3B4504A_INCLUDED
