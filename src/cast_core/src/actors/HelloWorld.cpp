#include <cast_core/actors/HelloWorld.hpp>

#include <string>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>

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
    : Actor(context),
      _outputTimer{context.timer("output", HelloWorld::id())},
      _operations{context.counter("operations", HelloWorld::id())},
      _loop{context} {}

namespace {
auto registerHelloWorld = genny::Cast::registerDefault<genny::actor::HelloWorld>();
}
