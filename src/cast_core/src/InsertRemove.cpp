// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cast_core/actors/InsertRemove.hpp>

#include <memory>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

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
              context.get<ExecutionStrategy::RunOptions,false>("InsertStage", "ExecutionsStrategy").value_or(ExecutionStrategy::RunOptions{}),
              context.get<ExecutionStrategy::RunOptions,false>("RemoveStage", "ExecutionsStrategy").value_or(ExecutionStrategy::RunOptions{})) {}

    mongocxx::database database;
    mongocxx::collection collection;
    bsoncxx::document::value myDoc;

    ExecutionStrategy::RunOptions insertOptions;
    ExecutionStrategy::RunOptions removeOptions;
};

void InsertRemove::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            BOOST_LOG_TRIVIAL(debug) << " Inserting and then removing";
            _insertStrategy.run(
                [&](metrics::OperationContext& ctx) {
                    // First we insert
                    auto view = config->myDoc.view();
                    config->collection.insert_one(view);
                    ctx.addBytes(view.length());
                    ctx.addDocuments(1);
                },
                config->insertOptions);
            _removeStrategy.run(
                [&](metrics::OperationContext& ctx) {
                    // Then we remove
                    auto results = config->collection.delete_many(config->myDoc.view());
                    ctx.addDocuments(1);
                },
                config->removeOptions);
        }
    }
}

InsertRemove::InsertRemove(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().getRNGForThread(InsertRemove::id())},
      _insertStrategy{context.operation("Insert", InsertRemove::id())},
      _removeStrategy{context.operation("Remove", InsertRemove::id())},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, InsertRemove::id()} {}

namespace {
auto registerInsertRemove = genny::Cast::registerDefault<genny::actor::InsertRemove>();
}
}  // namespace genny::actor
