#include <algorithm>  // std::max
#include <cassert>
#include <iostream>

#include <gennylib/Orchestrator.hpp>

namespace {

// so we can call morePhases() in awaitPhaseEnd.
inline constexpr bool morePhaseLogic(genny::PhaseNumber currentPhase,
                                     genny::PhaseNumber maxPhase,
                                     bool errors) {
    return currentPhase <= maxPhase && !errors;
}

}  // namespace


namespace genny {

/*
 * Multiple readers can access at same time, but only one writer.
 * Use reader when only reading internal state, but use writer whenever changing it.
 */
using reader = std::shared_lock<std::shared_mutex>;
using writer = std::unique_lock<std::shared_mutex>;

PhaseNumber Orchestrator::currentPhase() const {
    reader lock{_mutex};

    return this->_current;
}

bool Orchestrator::continueRunning() const {
    reader lock{_mutex};
    return !this->_errors;
}

bool Orchestrator::morePhases() const {
    reader lock{_mutex};

    return morePhaseLogic(this->_current, this->_max, this->_errors);
}

// we start once we have required number of tokens
PhaseNumber Orchestrator::awaitPhaseStart(bool block, int addTokens) {
    writer lock{_mutex};
    assert(state == State::PhaseEnded);

    _currentTokens += addTokens;

    auto currentPhase = this->_current;
    if (_currentTokens >= _requireTokens) {
        _phaseChange.notify_all();
        state = State::PhaseStarted;
    } else {
        if (block) {
            while (state != State::PhaseStarted) {
                _phaseChange.wait(lock);
            }
        }
    }
    return currentPhase;
}

void Orchestrator::addRequiredTokens(int tokens) {
    writer lock{_mutex};

    this->_requireTokens += tokens;
}

void Orchestrator::phasesAtLeastTo(PhaseNumber minPhase) {
    writer lock{_mutex};
    this->_max = std::max(this->_max, minPhase);
}

// we end once no more tokens left
bool Orchestrator::awaitPhaseEnd(bool block, int removeTokens) {
    writer lock{_mutex};
    assert(State::PhaseStarted == state);

    _currentTokens -= removeTokens;

    // Not clear if we should allow _currentTokens to drop below zero
    // and if below check should be `if (_currentTokens == 0)`.
    //
    // - defensive programming says that a bug could cause it to go below zero
    //   and if that happens and we're comparing == 0, then the workload will
    //   block forever
    //
    // - an Actor could get clever by wanting the tokens to dip below zero if
    //   it knows it will level-them out or whatever in the future
    //
    // - BUT: there's no real existing good reason why an actor would
    //   want to do this, so it's likely an error. Presumably such errors
    //   will be caught in automated testing, though, so adding a runtime
    //   check seems to limit the functionality unnecessarily.
    //
    // Similar thing applies to the block in awaitPhaseStart() where we
    // compare with >= rather than ==.

    if (_currentTokens <= 0) {
        ++_current;
        _phaseChange.notify_all();
        state = State::PhaseEnded;
    } else {
        if (block) {
            while (state != State::PhaseEnded) {
                _phaseChange.wait(lock);
            }
        }
    }
    return morePhaseLogic(this->_current, this->_max, this->_errors);
}

void Orchestrator::abort() {
    writer lock{_mutex};

    this->_errors = true;
}

}  // namespace genny
