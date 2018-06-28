#include <utility>

#include <gennylib/PhasedActor.hpp>

#include "log.hh"


void genny::PhasedActor::run() {
    while (_context.orchestrator()->morePhases()) {
        _context.orchestrator()->awaitPhaseStart();

        try {
            this->phase(_context.orchestrator()->currentPhaseNumber());
        } catch (const std::exception& ex) {
            BOOST_LOG_TRIVIAL(error) << "Exception " << ex.what();
            _context.orchestrator()->abort();
        }

        // wait for phase to end before proceeding
        _context.orchestrator()->awaitPhaseEnd();
    }
}

genny::PhasedActor::PhasedActor(genny::ActorContext& context, std::string name)
: _context{context}, _name{std::move(name)} {}
