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
    unsigned int currentPhaseNumber() const;

    /**
     * @return if there are any more phases.
     */
    bool morePhases() const;

    /**
     * @param minPhase the minimum phase number that the Orchestrator should run to.
     */
    void phasesAtLeastTo(unsigned int minPhase);

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
    unsigned int awaitPhaseStart(bool block = true, int addTokens = 1);

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
     *     int phase = orchestrator.awaitPhaseStart();
     *     orchestrator.awaitPhaseEnd(false);
     *     while(phase == orchestrator.currentPhaseNumber()) {
     *         // do operation
     *     }
     * }
     * ```
     */
    bool awaitPhaseEnd(bool block = true, int removeTokens = 1);

    void addRequiredTokens(int tokens);

    void abort();

    V1::OrchestratorLoop loop(std::unordered_map<long, bool> blockingPhases);

private:
    mutable std::shared_mutex _mutex;
    std::condition_variable_any _cv;

    int _requireTokens = 0;
    int _currentTokens = 0;

    unsigned int _maxPhase = 1;
    unsigned int _phase = 0;

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
                              std::unordered_map<long, bool> blockingPhases);
    OrchestratorIterator begin();
    OrchestratorIterator end();

    // no copy
    OrchestratorLoop(OrchestratorLoop&) = delete;
    void operator=(OrchestratorLoop&) = delete;

private:
    friend OrchestratorIterator;
    bool morePhases() const;
    bool doesBlockOn(int phase) const;

    Orchestrator* _orchestrator;
    std::unordered_map<long, bool> _blockingPhases;
};

// returned from orchestrator.loop().begin()
// and           orchestrator.loop().end()
class OrchestratorIterator {
public:
    // <iterator-concept>
    typedef std::forward_iterator_tag iterator_category;
    typedef int value_type;
    typedef int reference;
    typedef int pointer;
    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

    explicit OrchestratorIterator(OrchestratorLoop&, bool);
    bool operator==(const OrchestratorIterator&) const;
    bool operator!=(const OrchestratorIterator&) const;
    int operator*();
    OrchestratorIterator& operator++();

private:
    friend OrchestratorLoop;
    bool atEnd() const;
    OrchestratorLoop* _loop;
    bool _isEnd;
};


}  // namespace V1


}  // namespace genny

#endif  // HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
