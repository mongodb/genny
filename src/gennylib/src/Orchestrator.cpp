#include <algorithm> // std::max
#include <cassert>
#include <iostream>
#include <shared_mutex>

#include <boost/log/trivial.hpp>

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

V1::OrchestratorLoop Orchestrator::loop(std::unordered_map<long, bool> blockingPhases) {
    return V1::OrchestratorLoop(*this, blockingPhases);
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

bool V1::OrchestratorLoop::doesBlockOn(int phase) const {
    if (auto it = _blockingPhases.find(phase); it != _blockingPhases.end()) {
        BOOST_LOG_TRIVIAL(info) << "does block on " << phase << "? " << it->second;
        return it->second;
    }
    BOOST_LOG_TRIVIAL(info) << "does block on " << phase << "? not found -> false";
    return false;
}

bool V1::OrchestratorLoop::morePhases() const {
    return this->_orchestrator->morePhases();
}

V1::OrchestratorIterator::OrchestratorIterator(V1::OrchestratorLoop & orchestratorLoop, bool isEnd)
: _loop{std::addressof(orchestratorLoop)},
  _isEnd{isEnd} {

    BOOST_LOG_TRIVIAL(info) << "iterator ctor for isEnd=" << isEnd;

}

/*
operator*  calls awaitPhaseStart (and immediately awaitEnd if non-blocking)
operator++ calls awaitPhaseEnd   (but not if blocking)
*/

int V1::OrchestratorIterator::operator*() {
    BOOST_LOG_TRIVIAL(info) << "start of operator* with currentPhase " << this->_loop->_orchestrator->currentPhaseNumber();
    auto phase = this->_loop->_orchestrator->awaitPhaseStart();
    BOOST_LOG_TRIVIAL(info) << "operator* phase from awaitStart: " << phase;
    if (this->_loop->doesBlockOn(phase)) {
        BOOST_LOG_TRIVIAL(info) << "operator* blocks on phase " << phase;
        this->_loop->_orchestrator->awaitPhaseEnd(false);
    }
    BOOST_LOG_TRIVIAL(info) << "end of operator* with currentPhase " << this->_loop->_orchestrator->currentPhaseNumber();
    return phase;
}

V1::OrchestratorIterator &V1::OrchestratorIterator::operator++() {
    auto phase = this->_loop->_orchestrator->currentPhaseNumber();
    BOOST_LOG_TRIVIAL(info) << "operator++ phase=" << phase;
    if (! this->_loop->doesBlockOn(phase)) {
        if (!this->atEnd()) {
            BOOST_LOG_TRIVIAL(info) << "operator* noblock on " << phase << " @ end";
            this->_loop->_orchestrator->awaitPhaseEnd(true);
        }
    }
    BOOST_LOG_TRIVIAL(info) << "end of operator++";
    return *this;
}

bool V1::OrchestratorIterator::atEnd() const {
    return !(_loop->morePhases());
}

bool V1::OrchestratorIterator::operator==(const V1::OrchestratorIterator &other) const {
    BOOST_LOG_TRIVIAL(info) << "operator== with other._isEnd " << other._isEnd << " and atEnd=" << this->atEnd();
    return (other._isEnd && this->atEnd()) ||
           (this->_isEnd && other.atEnd()) ||
           (this == &other) ||
           (this->_isEnd && other._isEnd && this->_loop == other._loop);
}

}  // namespace genny
