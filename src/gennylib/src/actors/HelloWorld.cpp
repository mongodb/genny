#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int) {
    auto op = _output_timer.raii();
    std::cout << _name << " Doing Phase " << _config.orchestrator()->currentPhaseNumber() << std::endl;
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(genny::ActorContext config, const std::string& name)
    : PhasedActor(config, name),
      _output_timer{config.timer("hello." + name + ".output")},
      _operations{config.counter("hello." + name + ".operations")} {};
