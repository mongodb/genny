#include <algorithm> // std::max
#include <cassert>
#include <iostream>
#include <shared_mutex>

#include <gennylib/Orchestrator.hpp>

namespace {

// so we can call morePhases() in awaitPhaseEnd.
inline constexpr bool morePhaseLogic(int currentPhase, int maxPhase, bool errors) {
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

unsigned int Orchestrator::currentPhaseNumber() const {
    reader lk{_mutex};

    return this->_phase;
}

bool Orchestrator::morePhases() const {
    reader lk{_mutex};

    return morePhaseLogic(this->_phase, this->_maxPhase, this->_errors);
}

// we start once we have required number of tokens
unsigned int Orchestrator::awaitPhaseStart(bool block, int addTokens) {
    writer lk{_mutex};

    assert(state == State::PhaseEnded);
    _currentTokens += addTokens;
    unsigned int out = this->_phase;
    if (_currentTokens >= _requireTokens) {
        _cv.notify_all();
        state = State::PhaseStarted;
    } else {
        if (block) {
            _cv.wait(lk);
        }
    }
    return out;
}

void Orchestrator::addRequiredTokens(int tokens) {
    writer lk{_mutex};

    this->_requireTokens += tokens;
}

void Orchestrator::phasesAtLeastTo(unsigned int minPhase) {
    writer lk{_mutex};
    this->_maxPhase = std::max(this->_maxPhase, minPhase);
}

// we end once no more tokens left
bool Orchestrator::awaitPhaseEnd(bool block, int removeTokens) {
    writer lk{_mutex};

    assert(State::PhaseStarted == state);
    _currentTokens -= removeTokens;
    if (_currentTokens <= 0) {
        ++_phase;
        _cv.notify_all();
        state = State::PhaseEnded;
    } else {
        if (block) {
            _cv.wait(lk);
        }
    }
    return morePhaseLogic(this->_phase, this->_maxPhase, this->_errors);
}

void Orchestrator::abort() {
    writer lk{_mutex};

    this->_errors = true;
}

}  // namespace genny
