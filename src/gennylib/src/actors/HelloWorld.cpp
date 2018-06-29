#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int) {
    auto op = _output_timer.raii();
    BOOST_LOG_TRIVIAL(info) << _name << " Doing Phase " << _context.orchestrator()->currentPhaseNumber();
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context, const std::string& name)
    : PhasedActor(context, name),
      _output_timer{context.timer("hello." + name + ".output")},
      _operations{context.counter("hello." + name + ".operations")} {};
