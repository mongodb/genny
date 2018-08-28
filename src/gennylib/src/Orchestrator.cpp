#include <cassert>
#include <iostream>
#include <shared_mutex>

#include <gennylib/Orchestrator.hpp>

namespace genny {

using reader = std::shared_lock<std::shared_mutex>;
using writer = std::unique_lock<std::shared_mutex>;

unsigned int Orchestrator::currentPhaseNumber() const {
    reader lk{_mutex};

    return this->_phase;
}

bool Orchestrator::morePhases() const {
    reader lk{_mutex};

    return this->_phase <= this->_maxPhase && !this->_errors;
}

int Orchestrator::awaitPhaseStart(bool block, int addTokens) {
    writer lk{_mutex};

    assert(state == State::PhaseEnded);
    _currentTokens += addTokens;
    int out = this->_phase;
    if (_currentTokens == _wantTokens) {
        _cv.notify_all();
        state = State::PhaseStarted;
    } else {
        if(block) {
            _cv.wait(lk);
        }
    }
    return out;
}

void Orchestrator::addTokens(int tokens) {
    writer lk{_mutex};

    this->_wantTokens += tokens;
}

void Orchestrator::awaitPhaseEnd(bool block, unsigned int morePhases, int removeTokens) {
    writer lk{_mutex};

    assert(State::PhaseStarted == state);
    this->_maxPhase += morePhases;
    _currentTokens -= removeTokens;
    if (_currentTokens == 0) {
        ++_phase;
        _cv.notify_all();
        state = State::PhaseEnded;
    } else {
        if (block) {
            _cv.wait(lk);
        }
    }
}

void Orchestrator::abort() {
    writer lk{_mutex};

    this->_errors = true;
}

}  // namespace genny
