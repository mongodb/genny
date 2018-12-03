#include <gennylib/actors/InsertRemove.hpp>

#include <memory>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

struct genny::actor::InsertRemove::PhaseConfig {
    PhaseConfig(mongocxx::database db,
                const std::string collection_name,
                std::mt19937_64& rng,
                int id)
        : database{db},
          collection{db[collection_name]},
          myDoc(bsoncxx::builder::stream::document{} << "_id" << id
                                                     << bsoncxx::builder::stream::finalize) {}
    PhaseConfig(PhaseContext& context,
                std::mt19937_64& rng,
                mongocxx::pool::entry& client,
                int id)
        : PhaseConfig((*client)[context.get<std::string>("Database")],
                      context.get<std::string>("Collection"),
                      rng,
                      id) {}
    mongocxx::database database;
    mongocxx::collection collection;
    bsoncxx::document::value myDoc;
};

void genny::actor::InsertRemove::run() {
    for (auto&& [phase, config] : _loop) {
        for (auto&& _ : config) {
            BOOST_LOG_TRIVIAL(info) << " Inserting and then removing";
            {
                auto op = _insertTimer.raii();
                config->collection.insert_one(config->myDoc.view());
            }
            {
                auto op = _removeTimer.raii();
                config->collection.delete_many(config->myDoc.view());
            }
        }
    }
}

genny::actor::InsertRemove::InsertRemove(genny::ActorContext& context)
    : _rng{context.workload().createRNG()},
      _insertTimer{context.timer("insert", Actor::id())},
      _removeTimer{context.timer("remove", Actor::id())},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, Actor::id()} {}

genny::ActorVector genny::actor::InsertRemove::producer(genny::ActorContext& context) {
    if (context.get<std::string>("Type") != "InsertRemove") {
        return {};
    }

    ActorVector out;

    auto threads = context.get<int>("Threads");
    for (int i = 0; i < threads; ++i) {
        out.push_back(std::make_unique<genny::actor::InsertRemove>(context));
    }

    return out;
}
