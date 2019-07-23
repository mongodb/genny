// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
#define HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED

#include <atomic>
#include <condition_variable>
#include <functional>
#include <shared_mutex>
#include <vector>

namespace genny {

class Orchestrator;

// May eventually want a proper type for Phase, but for now just a typedef is sufficient.
using PhaseNumber = unsigned int;

using OrchestratorCB = std::function<void(const Orchestrator*)>;

/**
 * Responsible for the synchronization of actors
 * across a workload's lifecycle.
 */
class Orchestrator {

public:
    // TODO TIG-1098: Writing `Orchestrator() = default` causes a performance regression in the
    // "Orchestrator Perf" benchmark where as low as 75% of `regIters` occur.
    explicit Orchestrator() {}

    /**
     * @return the current phase number
     */
    PhaseNumber currentPhase() const;

    /**
     * @return if there are any more phases.
     */
    bool morePhases() const;

    /**
     * @param minPhase the minimum phase number that the Orchestrator should run to.
     */
    void phasesAtLeastTo(PhaseNumber minPhase);

    /**
     * Signal from an actor that it is ready to start the next phase.
     *
     * The current phase is started when the current number of tokens
     * equals the required number of tokens. This is usually the
     * total number of Actors (each Actor owns a token).
     *
     * @param block if the call should block waiting for other callers.
     * @param addTokens the number of tokens added by this call.
     * @return the phase that has just started.
     */
    PhaseNumber awaitPhaseStart(bool block = true, int addTokens = 1);

    /**
     * Signal from an actor that it is done with the current phase.
     * Optionally blocks until the phase is ended when all actors report they are done.
     *
     * @param block whether or not this call should block until phase is ended
     * This can be used to make actors work "in the background" either across
     * phases or in an "optimistic" fashion such that long-running operations
     * don't cause the phase-progression to stall.
     * @param removeTokens how many "tokens" should be removed.
     * @see awaitPhaseStart()
     *
     * ```c++
     * while (orchestrator.morePhases()) {
     *     auto phase = orchestrator.awaitPhaseStart();
     *     orchestrator.awaitPhaseEnd(false);
     *     while(phase == orchestrator.currentPhase()) {
     *         // do operation
     *     }
     * }
     * ```
     */
    bool awaitPhaseEnd(bool block = true, int removeTokens = 1);

    void addRequiredTokens(int tokens);

    void abort();

    void addPrePhaseStartHook(const OrchestratorCB& f);

    /**
     * @return whether the workload should continue running. This is true as long as
     * no calls to abort() have been made.
     */
    bool continueRunning() const;


private:
    mutable std::shared_mutex _mutex;
    std::condition_variable_any _phaseChange;

    int _requireTokens = 0;
    int _currentTokens = 0;

    PhaseNumber _max = 0;
    PhaseNumber _current = 0;

    // Having this lets us avoid locking on _mutex for every call of
    // continueRunning(). This gave two orders of magnitude speedup.
    std::atomic_bool _errors = false;

    enum class State { PhaseEnded, PhaseStarted };

    State state = State::PhaseEnded;

    std::vector<OrchestratorCB> _prePhaseHooks;
};

}  // namespace genny

#endif  // HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
