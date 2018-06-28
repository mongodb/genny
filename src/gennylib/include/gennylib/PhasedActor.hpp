#ifndef HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED
#define HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED

#include <string>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

/**
 * The basic extension point for actors that want to vary
 * their behavior over the course of a workload.
 */
class PhasedActor : public Actor {

public:
    explicit PhasedActor(genny::ActorContext& context, std::string name = "anonymous");

    ~PhasedActor() override = default;

    /**
     * Wrapper to {@code doPhase()}. Not virtual so this parent class can add
     * before/after behavior in the future. See the documentation for
     * {@code doPhase()}.
     *
     * @param currentPhaseNumber the current phase number.
     */
    void phase(const int currentPhaseNumber) {
        this->doPhase(currentPhaseNumber);
    }

    /**
     * The "main" method of the actor. This should only be called by workload
     * drivers.
     */
    void run() final;

protected:
    ActorContext& _context;
    const std::string _name;

private:
    /**
     * An actor must implement this method.
     * @param currentPhaseNumber the current phase number
     */
    virtual void doPhase(int currentPhaseNumber) = 0;
};


}  // namespace genny

#endif  // HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED
