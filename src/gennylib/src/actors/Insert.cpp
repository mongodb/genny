#include "log.hh"

#include <gennylib/actors/Insert.hpp>

struct genny::actor::Insert::Config {

    Config(genny::ActorContext& context) : database{context.get<std::string>("Database")} {
        YAML::Node node_documents = context.get("Documents");
        for (const auto& node : context.get("Documents")) {
            std::string collection = node["Collection"].as<std::string>();
            std::string document = node["Document"].as<std::string>();
            bsoncxx::document::view_or_value json_doc = bsoncxx::from_json(document);
            documents.push_back(std::make_tuple(collection, json_doc, document));
        }
    }
    std::string database;
    std::vector<std::tuple<std::string, bsoncxx::document::view_or_value, std::string>> documents;
};

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    auto collection = _db[std::get<0>(_config->documents[currentPhase])];
    auto document = std::get<1>(_config->documents[currentPhase]);
    auto string_document = std::get<2>(_config->documents[currentPhase]);
    BOOST_LOG_TRIVIAL(info) << _fullName << " Inserting " << string_document;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res = collection.insert_one(document);
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _outputTimer{context.timer(_fullName + ".output")},
      _operations{context.counter(_fullName + ".operations")},
      _config{std::make_unique<Config>(context)} {
    auto client = context.client();
    _db = (*client)[_config->database];
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
