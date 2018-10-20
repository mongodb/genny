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
                        auto op = h->_outputTimer.raii();
            BOOST_LOG_TRIVIAL(info) << "Doing PhaseNumber " << p << " " << h->_message;
            //            h->_operations.incr();
        }
    }
}

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

genny::actor::HelloWorld::HelloWorld(genny::ActorContext& context, unsigned int thread)
    : _loop{context, thread},
    _outputTimer{context.timer(context.get<std::string>("Name") + "." + std::to_string)} {}
