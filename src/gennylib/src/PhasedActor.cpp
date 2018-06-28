#include <utility>

#include <gennylib/PhasedActor.hpp>

#include "log.hh"


void genny::PhasedActor::run() {
    while (_config.orchestrator()->morePhases()) {
        _config.orchestrator()->awaitPhaseStart();

        try {
            this->phase(_config.orchestrator()->currentPhaseNumber());
        } catch (const std::exception& ex) {
            BOOST_LOG_TRIVIAL(error) << "Exception " << ex.what();
            _config.orchestrator()->abort();
        }

        // wait for phase to end before proceeding
        _config.orchestrator()->awaitPhaseEnd();
    }
}

genny::PhasedActor::PhasedActor(genny::ActorContext config, std::string name)
: _config{config}, _name{std::move(name)} {}