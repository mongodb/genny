#ifndef HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
#define HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED

#include <condition_variable>
#include <mutex>
#include <thread>

namespace genny {

/**
 * Responsible for the synchronization of actors
 * across a workload's lifecycle.
 */
class Orchestrator {

public:
    /**
     * @param actors
     *  The number of actors that will participate in phase lifecycle methods
     *  such as {@code awaitPhaseStart} and {@code awaitPhaseEnd}.
     */
    explicit Orchestrator(unsigned int actors) : _actors(actors) {}

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
     */
    void awaitPhaseStart();

    /**
     * Signal from an actor that it is done with the current phase.
     * Blocks until the phase is ended when all actors report they are done.
     */
    void awaitPhaseEnd();

    void abort();

private:
    mutable std::mutex _lock;
    std::condition_variable _cv;

    const unsigned int _actors;
    unsigned int _phase = 0;
    unsigned int _running = 0;
    bool _errors = false;
    enum class State { PhaseEnded, PhaseStarted };
    State state = State::PhaseEnded;
};


}  // namespace genny

#endif  // HEADER_8615FA7A_9344_43E1_A102_889F47CCC1A6_INCLUDED
