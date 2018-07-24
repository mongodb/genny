#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    BOOST_LOG_TRIVIAL(info) << _name << " Doing Phase "
                            << currentPhase << " " << _message;
    _operations.incr();
}

genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context, const std::string& name)
    : PhasedActor(context, name),
      _outputTimer{context.timer("hello." + name + ".output")},
      _operations{context.counter("hello." + name + ".operations")},
      _message{context.get<std::string>("Parameters", "Message")} {}

genny::ActorVector genny::actor::HelloWorld::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "HelloWorld") {
        return out;
    }
    const auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(context, std::to_string(i)));
    }
    return out;
}
