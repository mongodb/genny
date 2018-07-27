#include "log.hh"

#include <gennylib/actors/Insert.hpp>

struct genny::actor::Insert::Config {

    struct PhaseConfig {
        PhaseConfig(const std::string collection_name,
                    const std::string document,
                    const mongocxx::database& db)
            : collection{db[collection_name]},
              json_document{bsoncxx::from_json(document)},
              string_document{document} {}
        mongocxx::collection collection;
        const bsoncxx::document::view_or_value json_document;
        const std::string string_document;
    };

    Config(const genny::ActorContext& context, const mongocxx::database& db) {
        for (const auto& node : context.get("Documents")) {
            const auto& collection_name = node["Collection"].as<std::string>();
            const auto& document = node["Document"].as<std::string>();
            phases.emplace_back(collection_name, document, db);
        }
    }
    std::vector<PhaseConfig> phases;
};

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    auto& phase = _config->phases[currentPhase];
    BOOST_LOG_TRIVIAL(info) << _fullName << " Inserting " << phase.string_document;
    phase.collection.insert_one(phase.json_document);
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _outputTimer{context.timer(_fullName + ".output")},
      _operations{context.counter(_fullName + ".operations")},
      _client{std::move(context.client())},
      _config{std::make_unique<Config>(context, (*_client)[context.get<std::string>("Database")])} {
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
