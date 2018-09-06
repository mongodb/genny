#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(PhaseNumber currentPhase) {
    auto op = _outputTimer.raii();
    BOOST_LOG_TRIVIAL(info) << _fullName << " Doing PhaseNumber " << currentPhase << " " << _message;
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _outputTimer{context.timer(_fullName + ".output")},
      _operations{context.counter(_fullName + ".operations")},
      _message{context.get<std::string>("Parameters", "Message")} {}

genny::ActorVector genny::actor::HelloWorld::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "HelloWorld") {
        return out;
    }
    const auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(context, i));
    }
    return out;
}
