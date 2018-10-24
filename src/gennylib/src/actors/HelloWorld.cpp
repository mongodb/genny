#include <string>

#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

struct genny::Actor::HelloWorld::PhaseConfig {
    std::string message;
    explicit PhaseConfig(PhaseContext& context)
    : message{context.get<std::string,false>("Message").value_or("Hello, World!")} {}
};

void genny::actor::HelloWorld::run() {
    for (auto&& [p, h] : _loop) {
        for (auto _ : h) {
            auto op = this->_outputTimer.raii();
            BOOST_LOG_TRIVIAL(info) << "Doing PhaseNumber " << p << " " << h->_message;
            //            h->_operations.incr();
        }
    }
}
genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context, const unsigned int thread)
    : _outputTimer{context.timer("output", thread)},
      _operations{context.counter("operations", thread)},
      _loop{context, thread}
      {}

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
