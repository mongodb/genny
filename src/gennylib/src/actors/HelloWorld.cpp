#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int) {
    auto op = _output_timer.raii();
    BOOST_LOG_TRIVIAL(info) << _name
        << " Doing Phase " << _context.orchestrator()->currentPhaseNumber()
        << " " << _message;
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context, const std::string& name)
    : PhasedActor(context, name),
      _output_timer{context.timer("hello." + name + ".output")},
      _operations{context.counter("hello." + name + ".operations")},
      _message{context.get<std::string>("Parameters","Message")} {}

std::vector<std::unique_ptr<genny::Actor>> genny::actor::HelloWorld::producer(genny::ActorContext &actorConfig) {
    const auto count = actorConfig["Count"].as<int>();
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    for (int i = 0; i < count; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(actorConfig, std::to_string(i)));
    }
    return out;
}

