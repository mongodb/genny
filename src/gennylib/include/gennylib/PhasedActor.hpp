#ifndef HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED
#define HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED

#include <string>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/config.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

class ActorConfig;

/**
 * The basic extension point for actors that want to vary
 * their behavior over the course of a workload.
 */
class PhasedActor : public Actor {

public:
    explicit PhasedActor(const genny::ActorConfig& config, std::string name = "anonymous");

    virtual ~PhasedActor() override = default;

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
    void run() override final;

protected:
    const ActorConfig& _config;
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
