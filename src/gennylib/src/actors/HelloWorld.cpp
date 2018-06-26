#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int) {
    auto op = _output_timer.raii();
    std::cout << _name << " Doing Phase " << _orchestrator->currentPhaseNumber() << std::endl;
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(genny::Orchestrator* orchestrator,
                                     genny::metrics::Registry* metrics,
                                     const std::string& name)
    : PhasedActor(orchestrator, metrics, name),
      _output_timer{metrics->timer("hello." + name + ".output")},
      _operations{metrics->counter("hello." + name + ".operations")} {};
