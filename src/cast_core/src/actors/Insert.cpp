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
#include <value_generators/value_generators.hpp>

namespace genny::actor {

/** @private */
struct Insert::PhaseConfig {
    mongocxx::collection collection;
    value_generators::UniqueExpression documentExpr;
    ExecutionStrategy::RunOptions options;

    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          documentExpr{value_generators::Expression::parseOperand(phaseContext.get("Document"))},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {}
};

void Insert::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            _strategy.run(
                [&](metrics::OperationContext& ctx) {
                    auto document = config->documentExpr->evaluate(_rng).getDocument();
                    BOOST_LOG_TRIVIAL(info) << " Inserting " << bsoncxx::to_json(document.view());
                    config->collection.insert_one(document.view());
                    ctx.addOps(1);
                    ctx.addBytes(document.view().length());
                },
                config->options);
        }
    }
}

Insert::Insert(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _strategy{context.operation("insert", Insert::id())},
      _client{std::move(context.client())},
      _loop{context, (*_client)[context.get<std::string>("Database")]} {}

namespace {
auto registerInsert = genny::Cast::registerDefault<genny::actor::Insert>();
}
}  // namespace genny::actor
