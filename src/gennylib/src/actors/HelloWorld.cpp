#include <string>

#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

struct genny::actor::HelloWorld::PhaseConfig {
    std::string message;
    explicit PhaseConfig(PhaseContext& context)
        : message{context.get<std::string, false>("Message").value_or("Hello, World!")} {}
};

void genny::actor::HelloWorld::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto _ : config) {
            auto op = this->_outputTimer.raii();
            BOOST_LOG_TRIVIAL(info) << config->message;
        }
    }
}

genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context)
    : _id{nextActorId()},
      _outputTimer{context.timer("output", _id)},
      _operations{context.counter("operations", _id)},
      _loop{context} {}

genny::ActorVector genny::actor::HelloWorld::producer(genny::ActorContext& context) {
    if (context.get<std::string>("Type") != "HelloWorld") {
        return {};
    }

    ActorVector out;
    out.emplace_back(std::make_unique<actor::HelloWorld>(context));
    return out;
}
