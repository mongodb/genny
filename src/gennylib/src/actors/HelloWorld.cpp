#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int) {
    auto op = _output_timer.raii();
    std::cout << _name << " Doing Phase " << _orchestrator->currentPhaseNumber() << std::endl;
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(const genny::ActorConfig& config, const std::string& name)
    : PhasedActor(config, name),
      _output_timer{config.registry()->timer("hello." + name + ".output")},
      _operations{config.registry()->counter("hello." + name + ".operations")} {};
