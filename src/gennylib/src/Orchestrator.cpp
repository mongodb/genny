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

V1::OrchestratorLoop Orchestrator::loop(const std::unordered_set<PhaseNumber>& blockingPhases) {
    return V1::OrchestratorLoop(*this, blockingPhases);
}

V1::OrchestratorLoop::OrchestratorLoop(Orchestrator& orchestrator,
                                       const std::unordered_set<PhaseNumber>& blockingPhases)
    : _orchestrator{std::addressof(orchestrator)}, _blockingPhases{blockingPhases} {}

V1::OrchestratorLoopIterator V1::OrchestratorLoop::end() {
    return V1::OrchestratorLoopIterator{*this, true};
}

V1::OrchestratorLoopIterator V1::OrchestratorLoop::begin() {
    return V1::OrchestratorLoopIterator{*this, false};
}

bool V1::OrchestratorLoop::doesBlockOn(PhaseNumber phase) const {
    return _blockingPhases.find(phase) != _blockingPhases.end();
}

bool V1::OrchestratorLoop::morePhases() const {
    return this->_orchestrator->morePhases();
}

V1::OrchestratorLoopIterator::OrchestratorLoopIterator(V1::OrchestratorLoop& orchestratorLoop,
                                                       bool isEnd)
    : _loop{std::addressof(orchestratorLoop)}, _isEnd{isEnd}, _currentPhase{0} {}


PhaseNumber V1::OrchestratorLoopIterator::operator*() {
    // Intentionally don't bother with cases where user didn't call operator++()
    // between invocations of operator*() and vice-versa.
    //
    //
    // This type is only intended to be used by range-based for-loops
    // and their equivalent expanded definitions
    // https://en.cppreference.com/w/cpp/language/range-for
    _currentPhase = this->_loop->_orchestrator->awaitPhaseStart();
    if (!this->_loop->doesBlockOn(_currentPhase)) {
        this->_loop->_orchestrator->awaitPhaseEnd(false);
    }
    return _currentPhase;
}


V1::OrchestratorLoopIterator& V1::OrchestratorLoopIterator::operator++() {
    // Intentionally don't bother with cases where user didn't call operator++()
    // between invocations of operator*() and vice-versa.
    //
    // This type is only intended to be used by range-based for-loops
    // and their equivalent expanded definitions
    // https://en.cppreference.com/w/cpp/language/range-for
    if (this->_loop->doesBlockOn(_currentPhase)) {
        this->_loop->_orchestrator->awaitPhaseEnd(true);
    }
    return *this;
}

bool V1::OrchestratorLoopIterator::operator==(const V1::OrchestratorLoopIterator& other) const {
    // Intentionally don't handle self-equality or other "normal" cases.
    //
    // This type is only intended to be used by range-based for-loops
    // and their equivalent expanded definitions
    // https://en.cppreference.com/w/cpp/language/range-for
    auto out = (other._isEnd && !_loop->morePhases());
    return out;
}

}  // namespace genny
