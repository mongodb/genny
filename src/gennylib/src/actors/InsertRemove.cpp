#include <gennylib/actors/InsertRemove.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

struct genny::actor::InsertRemove::Config {

    struct PhaseConfig {
        PhaseConfig(const std::string collection_name,
                    std::mt19937_64& rng,
                    const mongocxx::database& db)
            : collection{db[collection_name]}, myDoc(
                                                     bsoncxx::builder::stream::document{} << 
                                                     "_id" <<  1 << bsoncxx::builder::stream::finalize)
        {}
        mongocxx::collection collection;
        bsoncxx::document::value myDoc;
    };

    Config(const genny::ActorContext& context, const mongocxx::database& db, std::mt19937_64& rng) {
        for (const auto& node : context.get("Phases")) {
            const auto& collection_name = node["Collection"].as<std::string>();
            phases.emplace_back(collection_name, rng, db);
        }
    }
    std::vector<PhaseConfig> phases;
};

void genny::actor::InsertRemove::doPhase(PhaseNumber currentPhase) {
    auto& phase = _config->phases[currentPhase];
    BOOST_LOG_TRIVIAL(info) << _fullName << " Inserting and then removing";
    {
        auto op = _insertTimer.raii();
        phase.collection.insert_one(phase.myDoc.view());
    }
    {
        auto op = _removeTimer.raii();
        phase.collection.delete_many(phase.myDoc.view());
    }
}

genny::actor::InsertRemove::InsertRemove(genny::ActorContext& context, const unsigned int thread)
    : PhasedActor(context, thread),
      _rng{context.workload().createRNG()},
      _insertTimer{context.timer(_fullName + ".insert")},
      _removeTimer{context.timer(_fullName + ".remove")},
      _client{std::move(context.client())},
      _config{std::make_unique<Config>(
          context, (*_client)[context.get<std::string>("Database")], _rng)} {}

genny::ActorVector genny::actor::InsertRemove::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "InsertRemove") {
        return out;
    }
    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::InsertRemove>(context, i));
    }
    return out;
}

