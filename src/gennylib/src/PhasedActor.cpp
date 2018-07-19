#include <utility>

#include <gennylib/PhasedActor.hpp>

#include "log.hh"


void genny::PhasedActor::run() {
    while (_context.morePhases()) {
        _context.awaitPhaseStart();

        try {
            this->phase(_context.currentPhaseNumber());
        } catch (const std::exception& ex) {
            BOOST_LOG_TRIVIAL(error) << "Exception " << ex.what();
            _context.abort();
        }

        // wait for phase to end before proceeding
        _context.awaitPhaseEnd();
    }
}

genny::PhasedActor::PhasedActor(genny::ActorContext& context, std::string name)
    : _context{context}, _name{std::move(name)} {}
