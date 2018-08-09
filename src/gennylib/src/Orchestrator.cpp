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

int Orchestrator::awaitPhaseStart(bool block, int callbacks) {
    std::unique_lock lck{_lock};
    assert(state == State::PhaseEnded);
    _running += callbacks;
    int out = this->_phase;
    if (_running == _numActors) {
        _cv.notify_all();
        state = State::PhaseStarted;
    } else {
        if(block) {
            _cv.wait(lck);
        }
    }
    return out;
}

void Orchestrator::registerCallbacks(int callbacks) {
    std::lock_guard lk(this->_lock);
    this->_numActors += callbacks;
}

void Orchestrator::awaitPhaseEnd(bool block, unsigned int morePhases, int callbacks) {
    std::unique_lock<std::mutex> lck{_lock};
    assert(State::PhaseStarted == state);
    this->_maxPhase += morePhases;
    _running -= callbacks;
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


Orchestrator::Loop::Loop(Orchestrator &_orchestrator, const int _start, const int _end, const bool _block)
        : _orchestrator(_orchestrator), _start(_start), _end(_end), _block(_block) {}

Orchestrator::PhaseIterator Orchestrator::Loop::begin() {
    return Orchestrator::PhaseIterator(_orchestrator, false, _start, _end, _block);
}

Orchestrator::PhaseIterator Orchestrator::Loop::end() {
    return Orchestrator::PhaseIterator(_orchestrator, true, _start, _end, _block);
}

Orchestrator::PhaseIterator::PhaseIterator(Orchestrator &_orchestrator, const bool _isEnd, const int _beginPhase,
                                           const int _endPhase, bool _block) : _orchestrator(_orchestrator),
                                                                               _isEnd(_isEnd), _beginPhase(_beginPhase),
                                                                               _endPhase(_endPhase), _block(_block) {}

int Orchestrator::PhaseIterator::operator*() const {
    return _orchestrator.currentPhaseNumber();
}

void Orchestrator::PhaseIterator::operator++() {
    _orchestrator.awaitPhaseEnd(_block);
    if (!_orchestrator.morePhases()) {
        return;
    }
    std::cerr << "Awaiting start of " << _orchestrator.awaitPhaseStart(_block) << std::endl;
}

Orchestrator::PhaseIterator::operator int() const {
    return _orchestrator.currentPhaseNumber();
}

bool Orchestrator::PhaseIterator::operator==(const Orchestrator::PhaseIterator &rhs) const {
    std::cerr << "Comparing " << _isEnd << " vs " << rhs._isEnd;
    return !_isEnd && rhs._isEnd && _orchestrator.morePhases();
}

}  // namespace genny
