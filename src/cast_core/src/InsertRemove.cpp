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
                int id)
        : database{db},
          collection{db[collection_name]},
          myDoc(bsoncxx::builder::stream::document{} << "_id" << id
                                                     << bsoncxx::builder::stream::finalize) {}

    PhaseConfig(PhaseContext& context,
                genny::DefaultRandom& rng,
                mongocxx::pool::entry& client,
                int id)
        : PhaseConfig((*client)[context["Database"].to<std::string>()],
                      context["Collection"].to<std::string>(),
                      rng,
                      id) {}

    mongocxx::database database;
    mongocxx::collection collection;
    bsoncxx::document::value myDoc;
};

void InsertRemove::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            BOOST_LOG_TRIVIAL(debug) << " Inserting and then removing";

            // First we insert
            auto insertCtx = _insert.start();
            auto view = config->myDoc.view();
            config->collection.insert_one(view);
            insertCtx.addBytes(view.length());
            insertCtx.addDocuments(1);
            insertCtx.success();

            // Then we remove
            auto removeCtx = _remove.start();
            auto results = config->collection.delete_many(config->myDoc.view());
            removeCtx.addDocuments(1);
            removeCtx.success();
        }
    }
}

InsertRemove::InsertRemove(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().getRNGForThread(InsertRemove::id())},
      _insert{context.operation("Insert", InsertRemove::id())},
      _remove{context.operation("Remove", InsertRemove::id())},
      _client{std::move(context.client())},
      _loop{context, _rng, _client, InsertRemove::id()} {}

namespace {
auto registerInsertRemove = genny::Cast::registerDefault<genny::actor::InsertRemove>();
}
}  // namespace genny::actor
