#include <gennylib/Orchestrator.hpp>

namespace genny {

unsigned int Orchestrator::currentPhaseNumber() const {
    std::lock_guard lk(this->_lock);
    return this->_phase;
}

bool Orchestrator::morePhases() const {
    std::lock_guard lk(this->_lock);
    return this->_phase <= 1 && !this->_errors;
}

void Orchestrator::awaitPhaseStart() {
    std::unique_lock lck{_lock};
    assert(state == State::PhaseEnded);
    ++_running;
    if (_running == _actors) {
        ++_phase;
        _cv.notify_all();
        state = State::PhaseStarted;
    } else {
        _cv.wait(lck);
    }
}

void Orchestrator::awaitPhaseEnd() {
    std::unique_lock<std::mutex> lck{_lock};
    assert(State::PhaseStarted == state);
    --_running;
    if (_running == 0) {
        _cv.notify_all();
        state = State::PhaseEnded;
    } else {
        _cv.wait(lck);
    }
}

void Orchestrator::abort() {
    std::unique_lock<std::mutex> lck{_lock};
    this->_errors = true;
}

void Orchestrator::setActors(const genny::ActorVector& actors) {
    std::unique_lock<std::mutex> lck{_lock};
    this->_actors = actors.size();
}

}  // namespace genny
