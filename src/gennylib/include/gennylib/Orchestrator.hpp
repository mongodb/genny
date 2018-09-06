#ifndef HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
#define HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED

#include <cassert>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <utility>


namespace genny {

using PhaseNumber = unsigned int;

namespace V1 {
class OrchestratorLoop;
}  // namespace V1


/**
 * Responsible for the synchronization of actors
 * across a workload's lifecycle.
 */
class Orchestrator {

public:
    explicit Orchestrator() = default;

    ~Orchestrator() = default;

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

    V1::OrchestratorLoop loop(std::unordered_map<PhaseNumber, bool> blockingPhases);

private:
    mutable std::shared_mutex _mutex;
    std::condition_variable_any _phaseChange;

    int _requireTokens = 0;
    int _currentTokens = 0;

    PhaseNumber _max = 1;
    PhaseNumber _current = 0;

    bool _errors = false;

    enum class State { PhaseEnded, PhaseStarted };

    State state = State::PhaseEnded;
};


namespace V1 {


class OrchestratorIterator;

// returned from orchestrator.loop()
class OrchestratorLoop {
public:
    explicit OrchestratorLoop(Orchestrator& orchestrator,
                              std::unordered_map<PhaseNumber, bool> blockingPhases);
    OrchestratorIterator begin();
    OrchestratorIterator end();

    // no copy
    OrchestratorLoop(OrchestratorLoop&) = delete;
    void operator=(OrchestratorLoop&) = delete;

private:
    friend OrchestratorIterator;
    bool morePhases() const;
    bool doesBlockOn(PhaseNumber phase) const;

    Orchestrator* _orchestrator;
    std::unordered_map<PhaseNumber, bool> _blockingPhases;
};

// returned from orchestrator.loop().begin()
// and           orchestrator.loop().end()
class OrchestratorIterator {
public:
    // <iterator-concept>
    typedef std::forward_iterator_tag iterator_category;
    typedef PhaseNumber value_type;
    typedef PhaseNumber reference;
    typedef PhaseNumber pointer;
    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

    explicit OrchestratorIterator(OrchestratorLoop&, bool);
    bool operator==(const OrchestratorIterator&) const;
    bool operator!=(const OrchestratorIterator& other) const {
        return !(*this == other);
    }
    PhaseNumber operator*();
    OrchestratorIterator& operator++();

private:
    friend OrchestratorLoop;
    OrchestratorLoop* _loop;
    bool _isEnd;
    PhaseNumber _currentPhase;
};


}  // namespace V1


}  // namespace genny

#endif  // HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
