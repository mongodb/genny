#include "log.hh"

#include <gennylib/actors/Insert.hpp>

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    auto coll = _context.get<std::string>("Documents", currentPhase, "Collection");
    auto doc = _context.get<std::string>("Documents", currentPhase, "Document");
    BOOST_LOG_TRIVIAL(info) << this->getFullName() << " Inserting " << doc;
    bsoncxx::document::view_or_value json_doc = bsoncxx::from_json(doc);
    mongocxx::collection collection = _db[coll];
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res = collection.insert_one(json_doc);
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context,
                             const unsigned int thread,
                             std::string mongoUri,
                             std::string dbName)
    : PhasedActor(context, thread),
      _mongoUri{mongoUri},
      _dbName{dbName},
      _outputTimer{context.timer(this->getFullName() + ".output")},
      _operations{context.counter(this->getFullName() + ".operations")} {
          BOOST_LOG_TRIVIAL(info) << mongoUri;
          mongocxx::uri uri(mongoUri);
          mongocxx::client client(uri);
          _db = client[dbName];
}

genny::ActorVector genny::actor::Insert::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "Insert") {
        return out;
    }
    mongocxx::instance instance{};
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::Insert>(context, i));
    }
    return out;
}
