#include <gennylib/Orchestrator.hpp>

namespace genny {

unsigned int Orchestrator::currentPhaseNumber() const {
    std::lock_guard lk(this->_lock);
    return this->_phase;
}

bool Orchestrator::morePhases() const {
    std::lock_guard lk(this->_lock);
    return this->_phase <= this->_maxPhase && !this->_errors;
}

int Orchestrator::awaitPhaseStart() {
    std::unique_lock lck{_lock};
    assert(state == State::PhaseEnded);
    ++_running;
    int out = this->_phase;
    if (_running == _numActors) {
        _cv.notify_all();
        state = State::PhaseStarted;
    } else {
        _cv.wait(lck);
    }
    return out;
}

void Orchestrator::awaitPhaseEnd(bool block, unsigned int morePhases) {
    std::unique_lock<std::mutex> lck{_lock};
    assert(State::PhaseStarted == state);
    this->_maxPhase += morePhases;
    --_running;
    if (_running == 0) {
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

void Orchestrator::setActors(const genny::ActorVector& actors) {
    std::unique_lock<std::mutex> lck{_lock};
    this->_numActors = actors.size();
}

}  // namespace genny
