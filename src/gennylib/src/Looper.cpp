#include <gennylib/Looper.hpp>

using namespace genny;

V1::OrchestratorLoop::OrchestratorLoop(genny::Orchestrator& orchestrator,
                                       const std::unordered_set<genny::PhaseNumber>& blockingPhases)
    : _orchestrator{std::addressof(orchestrator)}, _blockingPhases{blockingPhases} {}

V1::OrchestratorLoopIterator V1::OrchestratorLoop::end() {
    return V1::OrchestratorLoopIterator{*this, true};
}

V1::OrchestratorLoopIterator V1::OrchestratorLoop::begin() {
    return V1::OrchestratorLoopIterator{*this, false};
}

bool V1::OrchestratorLoop::doesBlockOn(genny::PhaseNumber phase) const {
    return _blockingPhases.find(phase) != _blockingPhases.end();
}

bool V1::OrchestratorLoop::morePhases() const {
    return this->_orchestrator->morePhases();
}

V1::OrchestratorLoopIterator::OrchestratorLoopIterator(V1::OrchestratorLoop& orchestratorLoop,
                                                       bool isEnd)
    : _loop{std::addressof(orchestratorLoop)},
      _isEnd{isEnd},
      _currentPhase{0},
      _awaitingPlusPlus{false} {}


PhaseNumber V1::OrchestratorLoopIterator::operator*() {
    assert(!_awaitingPlusPlus);

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

    _awaitingPlusPlus = true;
    return _currentPhase;
}


V1::OrchestratorLoopIterator& V1::OrchestratorLoopIterator::operator++() {
    assert(_awaitingPlusPlus);
    // Intentionally don't bother with cases where user didn't call operator++()
    // between invocations of operator*() and vice-versa.
    //
    // This type is only intended to be used by range-based for-loops
    // and their equivalent expanded definitions
    // https://en.cppreference.com/w/cpp/language/range-for
    if (this->_loop->doesBlockOn(_currentPhase)) {
        this->_loop->_orchestrator->awaitPhaseEnd(true);
    }

    _awaitingPlusPlus = false;
    return *this;
}

bool V1::OrchestratorLoopIterator::operator!=(const V1::OrchestratorLoopIterator& other) const {
    // Intentionally don't handle self-equality or other "normal" cases.
    //
    // This type is only intended to be used by range-based for-loops
    // and their equivalent expanded definitions
    // https://en.cppreference.com/w/cpp/language/range-for
    return !(other._isEnd && !_loop->morePhases());
}
