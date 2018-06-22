#include <gennylib/Orchestrator.hpp>

namespace genny {


Orchestrator::Orchestrator(unsigned long actors)
    : _actors{actors}, _phase{0}, _running{0}, _errors{false} {}

Orchestrator::~Orchestrator() = default;

unsigned int Orchestrator::currentPhaseNumber() const {
    return this->_phase;
}

bool Orchestrator::morePhases() const {
    return this->_phase <= 1 && !this->_errors;
}

void Orchestrator::awaitPhaseStart() {
    std::unique_lock<std::mutex> lck{_lock};
    ++_running;
    if (_running == _actors) {
        ++_phase;
        _cv.notify_all();
    } else {
        _cv.wait(lck);
    }
}

void Orchestrator::awaitPhaseEnd() {
    std::unique_lock<std::mutex> lck{_lock};
    --_running;
    if (_running == 0) {
        _cv.notify_all();
    } else {
        _cv.wait(lck);
    }
}

void Orchestrator::abort() {
    std::unique_lock<std::mutex> lck{_lock};
    this->_errors = true;
}

void Orchestrator::setActors(unsigned long actors) {
    std::unique_lock<std::mutex> lck{_lock};
    this->_actors = actors;
}

}  // namespace genny
