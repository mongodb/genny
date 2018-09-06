#include <utility>

#include <gennylib/PhasedActor.hpp>

#include "log.hh"


void genny::PhasedActor::run() {
    while (_context.morePhases()) {
        _context.awaitPhaseStart();

        try {
            this->phase(_context.currentPhase());
        } catch (const std::exception& ex) {
            BOOST_LOG_TRIVIAL(error) << "Exception " << ex.what();
            _context.abort();
        }

        // wait for phase to end before proceeding
        _context.awaitPhaseEnd();
    }
}

genny::PhasedActor::PhasedActor(genny::ActorContext& context, unsigned int thread)
    : _context{context},
      _thread{thread},
      _name{context.get<std::string>("Name")},
      _fullName{_name + "." + std::to_string(_thread)},
      _type{context.get<std::string>("Type")} {}
