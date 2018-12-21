#include <cast_core/actors/HelloWorld.hpp>

#include <string>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>

namespace genny::actor {

struct HelloWorld::PhaseConfig {
    std::string message;
    explicit PhaseConfig(PhaseContext& context)
        : message{context.get<std::string, false>("Message").value_or("Hello, World!")} {}
};

void HelloWorld::run() {
    for (auto&& config : _loop) {
        for (auto _ : config) {
            auto ctx = this->_operation.start();
            BOOST_LOG_TRIVIAL(info) << config->message;
            ++_hwCounter;
            BOOST_LOG_TRIVIAL(info) << "Counter: " << _hwCounter;
            ctx.setOps(1);
            ctx.setBytes(config->message.size());
        }
    }
}

HelloWorld::HelloWorld(genny::ActorContext& context)
    : Actor(context),
      _operation{context.operation("op", HelloWorld::id())},
      _hwCounter{WorkloadContext::getActorSharedState<HelloWorld, HelloWorldCounter>()},
      _loop{context} {}

namespace {
auto registerHelloWorld = genny::Cast::registerDefault<genny::actor::HelloWorld>();
}
}  // namespace genny::actor
