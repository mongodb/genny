#include <cast_core/actors/InsertRemove.hpp>

#include <memory>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace genny::actor {
struct InsertRemove::PhaseConfig {
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

void InsertRemove::run() {
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

InsertRemove::InsertRemove(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _insertTimer{context.timer("insert", InsertRemove::id())},
      _removeTimer{context.timer("remove", InsertRemove::id())},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, InsertRemove::id()} {}

namespace {
auto registerInsertRemove = genny::Cast::registerDefault<genny::actor::InsertRemove>();
}
}  // namespace genny::actor


