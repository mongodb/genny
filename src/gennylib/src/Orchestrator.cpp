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

#include <gennylib/Orchestrator.hpp>

#include <boost/log/trivial.hpp>

#include <algorithm>  // std::max
#include <cassert>

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
/** @private */
using reader = std::shared_lock<std::shared_mutex>;
/** @private */
using writer = std::unique_lock<std::shared_mutex>;

PhaseNumber Orchestrator::currentPhase() const {
    reader lock{_mutex};

    return this->_current;
}

bool Orchestrator::continueRunning() const {
    // Be careful when changing this.
    //
    // In particular, stay away from changes that want to add
    //     reader lock{_mutex}
    // here. This is a performance-killer, so the _errors state
    // isn't guarded by the Orchestrator's mutex.
    //
    // This method is called in a tight loop by every Actor
    // for every iteration of its inner `for(auto&& _ : cfg)` loop.
    //
    // Hence only allowed to read std::atomic values.
    //
    // PhaseLoop_perf_test.cpp deals heavily with the performance
    // implications of this method.
    //
    return !this->_errors;
}

bool Orchestrator::morePhases() const {
    reader lock{_mutex};

    return morePhaseLogic(this->_current, this->_max, this->_errors);
}

// we start once we have required number of tokens
PhaseNumber Orchestrator::awaitPhaseStart(bool block, int addTokens) {
    writer lock{_mutex};
    assert(state == State::PhaseEnded || this->_errors);

    _currentTokens += addTokens;

    const auto currentPhase = this->_current;
    if (_currentTokens >= _requireTokens) {
        for (auto&& cb : _prePhaseHooks) {
            cb(this);
        }
        BOOST_LOG_TRIVIAL(debug) << "Beginning phase " << currentPhase;
        _phaseChange.notify_all();
        state = State::PhaseStarted;
    } else {
        if (block) {
            while (state != State::PhaseStarted && !this->_errors) {
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
    assert(State::PhaseStarted == state || this->_errors);

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
        BOOST_LOG_TRIVIAL(debug) << "Ended phase " << (this->_current - 1);
        _phaseChange.notify_all();
        state = State::PhaseEnded;
    } else {
        if (block) {
            while (state != State::PhaseEnded && !this->_errors) {
                _phaseChange.wait(lock);
            }
        }
    }
    return morePhaseLogic(this->_current, this->_max, this->_errors);
}


void Orchestrator::addPrePhaseStartHook(const OrchestratorCB& f) {
    _prePhaseHooks.push_back(f);
}

void Orchestrator::abort() {
    writer lock{_mutex};
    this->_errors = true;
    _phaseChange.notify_all();
}

void Orchestrator::sleepToPhaseEnd(const PhaseNumber pn, Duration timeout) {
    reader lock{_mutex};
    if (this->_current != pn || state == State::PhaseEnded) {
        return;
    }
    _phaseChange.wait_for(lock, timeout);
}

}  // namespace genny
