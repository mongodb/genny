#include "log.hh"

#include <gennylib/actors/HelloWorld.hpp>

void genny::actor::HelloWorld::doPhase(PhaseNumber currentPhase) {
    auto op = _outputTimer.raii();
    BOOST_LOG_TRIVIAL(info) << _fullName << " Doing PhaseNumber " << currentPhase << " "
                            << _message;
    _operations.incr();
}

void genny::actor::HelloWorld::run() {
    for (auto&& [p, h] : _loop) {
        for (auto _ : h) {
            //            auto op = h->_outputTimer.raii();
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
    : _loop{context, thread} {}
