#ifndef HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
#define HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

namespace genny {

// May eventually want a proper type for Phase, but for now just a typedef is sufficient.
using PhaseNumber = unsigned int;

namespace V1 {
// needs to be pre-declared here; see usage in Orchestrator for details
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

    /**
     * @attention Only use this in range-based for loops.
     *
     * Iterates over all phases and will correctly call
     * `awaitPhaseStart()` and `awaitPhaseEnd()` in the
     * correct operators.
     *
     * ```c++
     * void run() {
     *     for(auto phase : orchestrator.loop({})) {
     *         while(phase == orchestrator.currentPhase()) {
     *             doOperation(phase);
     *         }
     *     }
     * }
     * ```
     *
     * This should **only** be used by range-based for loops because
     * the implementation relies on callers alternating between
     * `operator*()` and `operator++()` to indicate the caller's
     * done-ness or readiness of the current/next phase.
     *
     * @param blockingPhases
     *      Which Phases should "block".
     *      Non-blocking means that the iterator will immediately call
     *      awaitPhaseEnd() right after calling awaitPhaseStart(). This
     *      will prevent the Orchestrator from waiting for this Actor
     *      to complete its operations in the current Phase.
     *
     *      Note that the Actor still needs to wait for the next Phase
     *      to start before going on to the next iteration of the loop.
     *      The common way to do this is to periodically check that
     *      the current Phase number (`Orchestrator::currentPhase()`)
     *      hasn't changed.
     *
     *      The `PhaseLoop` type will soon be incorporated into this type
     *      and will support automatically doing this check if required.
     *
     */
    V1::OrchestratorLoop loop(const std::unordered_set<PhaseNumber>& blockingPhases);

private:
    mutable std::shared_mutex _mutex;
    std::condition_variable_any _phaseChange;

    int _requireTokens = 0;
    int _currentTokens = 0;

    PhaseNumber _max = 0;
    PhaseNumber _current = 0;

    bool _errors = false;

    enum class State { PhaseEnded, PhaseStarted };

    State state = State::PhaseEnded;
};


/*
 * Reminder: the V1 namespace types are *not* intended to be used directly.
 */
namespace V1 {


class OrchestratorLoopIterator;


// returned from orchestrator.loop()
class OrchestratorLoop {

public:
    OrchestratorLoopIterator begin();
    OrchestratorLoopIterator end();

private:
    friend Orchestrator;
    friend OrchestratorLoopIterator;

    OrchestratorLoop(Orchestrator& orchestrator,
                     const std::unordered_set<PhaseNumber>& blockingPhases);

    bool morePhases() const;
    bool doesBlockOn(PhaseNumber phase) const;

    Orchestrator* _orchestrator;
    const std::unordered_set<PhaseNumber>& _blockingPhases;
};


// Only usable in range-based for loops.
class OrchestratorLoopIterator {

public:
    // These are intentionally commented-out because this type
    // should not be used by any std algorithms that may rely on them.
    // This type should only be used by range-based for loops (which doesn't
    // rely on these typedefs). This should *hopefully* prevent some cases of
    // accidental mis-use.
    //
    // Decided to leave this code commented-out rather than deleting it
    // partially to document this shortcoming explicitly but also in case
    // we want to support the full concept in the future.
    // https://en.cppreference.com/w/cpp/named_req/InputIterator
    //
    // <iterator-concept>
    //    typedef std::forward_iterator_tag iterator_category;
    //    typedef PhaseNumber value_type;
    //    typedef PhaseNumber reference;
    //    typedef PhaseNumber pointer;
    //    typedef std::ptrdiff_t difference_type;
    // </iterator-concept>

    bool operator!=(const OrchestratorLoopIterator& other) const;

    PhaseNumber operator*();

    // TODO: add boolean to check if being used correctly

    OrchestratorLoopIterator& operator++();

private:
    friend OrchestratorLoop;

    explicit OrchestratorLoopIterator(OrchestratorLoop&, bool);

    OrchestratorLoop* _loop;
    bool _isEnd;
    PhaseNumber _currentPhase;
};


}  // namespace V1


}  // namespace genny

#endif  // HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
