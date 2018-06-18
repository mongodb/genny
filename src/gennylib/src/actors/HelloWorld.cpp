#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int currentPhase) {
    auto op = _cout.raii();
    std::cout << _name << " Doing Phase " << _orchestrator->currentPhaseNumber() << std::endl;
    _operations.incr();
}


genny::actor::HelloWorld::~HelloWorld() = default;

genny::actor::HelloWorld::HelloWorld(genny::Orchestrator& orchestrator, genny::metrics::Registry& metrics, const std::string& name)
    : PhasedActor(orchestrator, metrics, name),
    _cout{metrics.timer("hello." + name + ".cout")},
    _operations{metrics.counter("hello." + name + ".operations")}
    {};
