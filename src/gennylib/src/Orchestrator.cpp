#include <algorithm>  // std::max
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
    reader lock{_mutex};

    return this->_phase;
}

bool Orchestrator::morePhases() const {
    reader lock{_mutex};

    return morePhaseLogic(this->_phase, this->_maxPhase, this->_errors);
}

// we start once we have required number of tokens
unsigned int Orchestrator::awaitPhaseStart(bool block, int addTokens) {
    writer lock{_mutex};
    assert(state == State::PhaseEnded);

    _currentTokens += addTokens;

    unsigned int currentPhase = this->_phase;
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

void Orchestrator::phasesAtLeastTo(unsigned int minPhase) {
    writer lock{_mutex};
    this->_maxPhase = std::max(this->_maxPhase, minPhase);
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
        ++_phase;
        _phaseChange.notify_all();
        state = State::PhaseEnded;
    } else {
        if (block) {
            while (state != State::PhaseEnded) {
                _phaseChange.wait(lock);
            }
        }
    }
    return morePhaseLogic(this->_phase, this->_maxPhase, this->_errors);
}

void Orchestrator::abort() {
    writer lock{_mutex};

    this->_errors = true;
}

// TODO: should this be a ref?
V1::OrchestratorLoop Orchestrator::loop(std::unordered_map<long, bool> blockingPhases) {
    return V1::OrchestratorLoop(*this, std::move(blockingPhases));
}

V1::OrchestratorLoop::OrchestratorLoop(Orchestrator &orchestrator, std::unordered_map<long, bool> blockingPhases)
: _orchestrator{std::addressof(orchestrator)},
  _blockingPhases{std::move(blockingPhases)} {}

V1::OrchestratorIterator V1::OrchestratorLoop::end() {
    return V1::OrchestratorIterator{*this, true};
}

V1::OrchestratorIterator V1::OrchestratorLoop::begin() {
    return V1::OrchestratorIterator{*this, false};
}

// TODO: make type of phase a consistent type - maybe struct Phase { operator int() {} }?
bool V1::OrchestratorLoop::doesBlockOn(int phase) const {
    if (auto it = _blockingPhases.find(phase); it != _blockingPhases.end()) {
        return it->second;
    }
    // TODO: is this the right default?
    return false;
}

bool V1::OrchestratorLoop::morePhases() const {
    return this->_orchestrator->morePhases();
}

V1::OrchestratorIterator::OrchestratorIterator(V1::OrchestratorLoop & orchestratorLoop, bool isEnd)
: _loop{std::addressof(orchestratorLoop)},
  _isEnd{isEnd},
  _currentPhase{0} {}

/*
operator*  calls awaitPhaseStart (and immediately awaitEnd if non-blocking)
operator++ calls awaitPhaseEnd   (but not if blocking)
*/

// TODO: handle multiple calls to operator*() without calls to operator++() between??
int V1::OrchestratorIterator::operator*() {
    _currentPhase = this->_loop->_orchestrator->awaitPhaseStart();
    if (! this->_loop->doesBlockOn(_currentPhase)) {
        this->_loop->_orchestrator->awaitPhaseEnd(false);
    }
    return _currentPhase;
}


// TODO: handle multiple calls to operator++() without calls to operator*() between?
V1::OrchestratorIterator &V1::OrchestratorIterator::operator++() {
    if (this->_loop->doesBlockOn(_currentPhase)) {
        this->_loop->_orchestrator->awaitPhaseEnd(true);
    }
    return *this;
}

// TODO: handle self-equality
// TODO: handle created from different _loops
bool V1::OrchestratorIterator::operator==(const V1::OrchestratorIterator &other) const {
    auto out = (other._isEnd && !_loop->morePhases());
    return out;
}

}  // namespace genny
