#include <utility>

#include <gennylib/PhasedActor.hpp>

#include "log.hh"


void genny::PhasedActor::run() {
    while (_orchestrator->morePhases()) {
        _orchestrator->awaitPhaseStart();

        try {
            this->phase(_orchestrator->currentPhaseNumber());
        } catch(const std::exception& ex) {
            BOOST_LOG_TRIVIAL(error) << "Exception " << ex.what();
            _orchestrator->abort();
        }

        // wait for phase to end before proceeding
        _orchestrator->awaitPhaseEnd();
    }
}

genny::PhasedActor::PhasedActor(genny::Orchestrator& orchestrator,
                                genny::metrics::Registry& registry,
                                std::string name)
    : _orchestrator{std::addressof(orchestrator)},
    _metrics{std::addressof(registry)},
    _name{std::move(name)}
    {}
