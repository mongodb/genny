#include "log.hh"

#include <gennylib/actors/Insert.hpp>

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    auto coll = _db[_context.get<std::string>("Documents", currentPhase, "Collection")];
    auto doc = _context.get<std::string>("Documents", currentPhase, "Document");
    BOOST_LOG_TRIVIAL(info) << this->getFullName() << " Inserting " << doc;
    bsoncxx::document::view_or_value json_doc = bsoncxx::from_json(doc);
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res = coll.insert_one(json_doc);
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _outputTimer{context.timer(this->getFullName() + ".output")},
      _operations{context.counter(this->getFullName() + ".operations")} {
    auto client = context.clientPool().acquire();
    _db = (*client)[context.get<std::string>("Database")];
}

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
