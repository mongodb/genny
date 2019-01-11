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

/** @private */
struct InsertRemove::PhaseConfig {
    PhaseConfig(mongocxx::database db,
                const std::string collection_name,
                genny::DefaultRandom& rng,
                int id,
                ExecutionStrategy::RunOptions insertOpts = {},
                ExecutionStrategy::RunOptions removeOpts = {})
        : database{db},
          collection{db[collection_name]},
          myDoc(bsoncxx::builder::stream::document{} << "_id" << id
                                                     << bsoncxx::builder::stream::finalize),
          insertOptions{std::move(insertOpts)},
          removeOptions{std::move(removeOpts)} {}

    PhaseConfig(PhaseContext& context,
                genny::DefaultRandom& rng,
                mongocxx::pool::entry& client,
                int id)
        : PhaseConfig(
              (*client)[context.get<std::string>("Database")],
              context.get<std::string>("Collection"),
              rng,
              id,
              ExecutionStrategy::getOptionsFrom(context, "InsertStage", "ExecutionsStrategy"),
              ExecutionStrategy::getOptionsFrom(context, "RemoveStage", "ExecutionsStrategy")) {}

    mongocxx::database database;
    mongocxx::collection collection;
    bsoncxx::document::value myDoc;

    ExecutionStrategy::RunOptions insertOptions;
    ExecutionStrategy::RunOptions removeOptions;
};

void InsertRemove::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            BOOST_LOG_TRIVIAL(info) << " Inserting and then removing";
            _insertStrategy.run(
                [&]() {
                    // First we insert
                    config->collection.insert_one(config->myDoc.view());
                },
                config->insertOptions);
            _removeStrategy.run(
                [&]() {
                    // Then we remove
                    config->collection.delete_many(config->myDoc.view());
                },
                config->removeOptions);
        }
    }
}

InsertRemove::InsertRemove(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _insertStrategy{context, InsertRemove::id(), "insert"},
      _removeStrategy{context, InsertRemove::id(), "remove"},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, InsertRemove::id()} {}

namespace {
auto registerInsertRemove = genny::Cast::registerDefault<genny::actor::InsertRemove>();
}
}  // namespace genny::actor
