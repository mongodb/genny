#ifndef HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
#define HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace genny {

/**
 * Responsible for the synchronization of actors
 * across a workload's lifecycle.
 */
class Orchestrator {

    struct PhaseIterator;

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
     * Signal from an actor that it is ready to start the next phase.
     * Blocks until the phase is started when all actors report they are ready.
     * @return the phase that has just started.
     */
    int awaitPhaseStart(bool block=true, int callbacks = 1);

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
    // TODO: should the bool instead be on awaitStart() to indicate "hey don't wait for me"?
    // TODO: should Orchestrator intsead be a type of Iterator?
    // for (auto phase : context.orchestratorLoop()) { ... } with
    // orchestratorLoop taking params to indicate blocking-ness or something?
    void awaitPhaseEnd(bool block = true, unsigned int morePhases = 0, int callbacks=1);

    void registerCallbacks(int callbacks);

    void abort();

    struct Loop {
        Orchestrator::PhaseIterator begin();
        Orchestrator::PhaseIterator end();

        Loop(Orchestrator &_orchestrator, const int _start, const int _end, const bool _block);

        Orchestrator& _orchestrator;
        const int _start;
        const int _end;
        const bool _block;
    };

    Loop loop(int start, int end, bool block) {
        return Loop{*this, start, end, block};
    }

private:
    struct PhaseIterator {
        operator int() const;
        int operator*() const;
        void operator++();
        bool operator==(const PhaseIterator&) const;

        PhaseIterator(Orchestrator &_orchestrator, const bool _isEnd, const int _beginPhase, const int _endPhase,
                      bool _block);

        Orchestrator& _orchestrator;
        const bool _isEnd;
        const int _beginPhase;
        const int _endPhase;
        bool _block;
    };


    mutable std::mutex _lock;
    std::condition_variable _cv;

    int _numActors = 0;
    unsigned int _maxPhase = 1;
    unsigned int _phase = 0;
    unsigned int _running = 0;
    bool _errors = false;
    enum class State { PhaseEnded, PhaseStarted };
    State state = State::PhaseEnded;
};


}  // namespace genny

#endif  // HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
