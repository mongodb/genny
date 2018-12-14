#include <string>

#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

namespace genny::actor {

struct HelloWorld::PhaseConfig {
    std::string message;
    explicit PhaseConfig(PhaseContext& context)
        : message{context.get<std::string, false>("Message").value_or("Hello, World!")} {}
};

void HelloWorld::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto _ : config) {
            auto op = this->_outputTimer.raii();
            BOOST_LOG_TRIVIAL(info) << config->message;
            _loopCounter++;
            BOOST_LOG_TRIVIAL(info) << "Counter: " << _loopCounter;

        }
    }
}

HelloWorld::HelloWorld(genny::ActorContext& context)
: Actor(context),
_outputTimer{context.timer("output", HelloWorld::id())},
_operations{context.counter("operations", HelloWorld::id())},
_loopCounter{context.workload().getActorSharedState<HelloWorld, HelloWorld::LoopCounter>()},
_loop{context} {}

genny::ActorVector HelloWorld::producer(genny::ActorContext& context) {
    if (context.get<std::string>("Type") != "HelloWorld") {
        return {};
    }

    ActorVector out;

    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::HelloWorld>(context));
    }

    return out;
}
} // namespace genny::actor


