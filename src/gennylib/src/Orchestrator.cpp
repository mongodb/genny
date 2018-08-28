#include <gennylib/Orchestrator.hpp>
#include <iostream>

namespace genny {

unsigned int Orchestrator::currentPhaseNumber() const {
    std::lock_guard lk(this->_lock);
    return this->_phase;
}

bool Orchestrator::morePhases() const {
    std::lock_guard lk(this->_lock);
    return this->_phase <= this->_maxPhase && !this->_errors;
}

int Orchestrator::awaitPhaseStart(bool block, int addTokens) {
    std::unique_lock lck{_lock};
    assert(state == State::PhaseEnded);
    _currentTokens += addTokens;
    int out = this->_phase;
    if (_currentTokens == _numTokens) {
        _cv.notify_all();
        state = State::PhaseStarted;
    } else {
        if(block) {
            _cv.wait(lck);
        }
    }
    return out;
}

void Orchestrator::addTokens(int tokens) {
    std::lock_guard lk(this->_lock);
    this->_numTokens += tokens;
}

void Orchestrator::awaitPhaseEnd(bool block, unsigned int morePhases, int removeTokens) {
    std::unique_lock<std::mutex> lck{_lock};
    assert(State::PhaseStarted == state);
    this->_maxPhase += morePhases;
    _currentTokens -= removeTokens;
    if (_currentTokens == 0) {
        ++_phase;
        _cv.notify_all();
        state = State::PhaseEnded;
    } else {
        if (block) {
            _cv.wait(lck);
        }
    }
}

void Orchestrator::abort() {
    std::unique_lock<std::mutex> lck{_lock};
    this->_errors = true;
}

}  // namespace genny
