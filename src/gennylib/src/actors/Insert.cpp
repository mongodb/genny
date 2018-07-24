#include "log.hh"

#include <gennylib/actors/Insert.hpp>

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    BOOST_LOG_TRIVIAL(info) << this->getFullName() << " Inserting " << currentPhase << " "
                            << _documents[currentPhase];
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _outputTimer{context.timer(this->getFullName() + ".output")},
      _operations{context.counter(this->getFullName() + ".operations")},
      _documents{context.get<std::vector<std::string>>("Documents")} {}

genny::ActorVector genny::actor::Insert::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "Insert") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::Insert>(context, i));
    }
    return out;
}
