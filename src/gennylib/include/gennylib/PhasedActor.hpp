#ifndef HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED
#define HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED

#include <string>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>

namespace genny {

/**
 * DEPRECATED
 * Use PhaseLoop and inherit from Actor directly.
 * See the InsertRemove Actor as an example.
 */
class PhasedActor : public Actor {

public:
    explicit PhasedActor(genny::ActorContext& context, unsigned int thread);

    virtual ~PhasedActor() = default;

    /**
     * Wrapper to {@code doPhase()}. Not virtual so this parent class can add
     * before/after behavior in the future. See the documentation for
     * {@code doPhase()}.
     *
     * @param current the current phase.
     */
    void phase(PhaseNumber current) {
        this->doPhase(current);
    }

    /**
     * The "main" method of the actor. This should only be called by workload
     * drivers.
     */
    void run() final;

protected:
    ActorContext& _context;
    const unsigned int _thread;
    std::string _name;
    std::string _fullName;
    std::string _type;

private:
    /**
     * An actor must implement this method.
     * @param currentPhase the current phase
     */
    virtual void doPhase(PhaseNumber currentPhase) = 0;
};


}  // namespace genny

#endif  // HEADER_A446E094_43EC_4401_884D_2044AA041A00_INCLUDED
