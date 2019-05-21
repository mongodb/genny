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

#include <cast_core/actors/Insert.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <bsoncxx/json.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/** @private */
struct Insert::PhaseConfig {
    mongocxx::collection collection;
    DocumentGenerator documentExpr;

    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db, ActorId id)
        : collection{db[phaseContext["Collection"].to<std::string>()]},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}
};

void Insert::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto ctx = _insert.start();
            auto document = config->documentExpr();
            BOOST_LOG_TRIVIAL(info) << " Inserting " << bsoncxx::to_json(document.view());
            config->collection.insert_one(document.view());
            ctx.addDocuments(1);
            ctx.addBytes(document.view().length());
            ctx.success();
        }
    }
}

Insert::Insert(genny::ActorContext& context)
    : Actor(context),
      _insert{context.operation("Insert", Insert::id())},
      _client{std::move(context.client())},
      _loop{context, (*_client)[context["Database"].to<std::string>()], Insert::id()} {}

namespace {
auto registerInsert = genny::Cast::registerDefault<genny::actor::Insert>();
}
}  // namespace genny::actor
