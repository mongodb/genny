#include "log.hh"

#include <gennylib/actors/Insert.hpp>

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    BOOST_LOG_TRIVIAL(info) << _name << " Inserting "
                            << currentPhase << " " << _documents[currentPhase];
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context, const std::string& name)
    : PhasedActor(context, name),
      _outputTimer{context.timer("insert." + name + ".output")},
      _operations{context.counter("insert." + name + ".operations")},
      _documents{context.get<std::vector<std::string>>("documents")} {}

genny::ActorVector genny::actor::Insert::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "Insert") {
        return out;
    }
    for (int i = 0; i < count; ++i) {
        out.push_back(std::make_unique<genny::actor::Insert>(context, std::to_string(i)));
    }
    return out;
}
