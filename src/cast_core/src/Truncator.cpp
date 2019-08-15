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

#include <cast_core/actors/Truncator.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {
struct Truncator::PhaseConfig {
    mongocxx::database database;
    mongocxx::collection collection;
    metrics::Operation truncateOperation;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db},
          collection{database[phaseContext["Collection"].to<std::string>()]},
          truncateOperation{context.operation("Truncate", id)} {}
};

void Truncator::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            config->truncateOperation.start();
            config->collection.find_one_and_delete({});
            config->truncateOperation.success();
        }
    }
}

Truncator::Truncator(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], Truncator::id()} {}

namespace {
auto registerTruncator = Cast::registerDefault<Truncator>();
}  // namespace
}  // namespace genny::actor
