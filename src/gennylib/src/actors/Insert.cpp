
#include <memory>
#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/actors/Insert.hpp>
#include <gennylib/context.hpp>
#include <gennylib/generators.hpp>

struct genny::actor::Insert::Config {

    struct PhaseConfig {
        PhaseConfig(const std::string collection_name,
                    const YAML::Node document_node,
                    std::mt19937_64& rng,
                    const mongocxx::database& db)
            : collection{db[collection_name]},
              json_document{generators::makeDoc(document_node, rng)} {}
        mongocxx::collection collection;
        std::unique_ptr<generators::DocumentGenerator> json_document;
    };

    Config(const genny::ActorContext& context, const mongocxx::database& db, std::mt19937_64& rng) {
        for (const auto& node : context.get("Phases")) {
            const auto& collection_name = node["Collection"].as<std::string>();
            phases.emplace_back(collection_name, node["Document"], rng, db);
        }
    }
    std::vector<PhaseConfig> phases;
};

void genny::actor::Insert::doPhase(int currentPhase) {
    auto op = _outputTimer.raii();
    auto& phase = _config->phases[currentPhase];
    bsoncxx::builder::stream::document mydoc{};
    auto view = phase.json_document->view(mydoc);
    BOOST_LOG_TRIVIAL(info) << _fullName << " Inserting " << bsoncxx::to_json(view);
    phase.collection.insert_one(view);
    _operations.incr();
}

genny::actor::Insert::Insert(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _rng{context.workload().createRNG()},
      _outputTimer{context.timer(_fullName + ".output")},
      _operations{context.counter(_fullName + ".operations")},
      _client{std::move(context.client())},
      _config{std::make_unique<Config>(
          context, (*_client)[context.get<std::string>("Database")], _rng)} {}

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
