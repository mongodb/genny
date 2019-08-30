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

#include <cast_core/actors/Deleter.hpp>

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
struct Deleter::PhaseConfig {
    mongocxx::database database;
    mongocxx::collection collection;
    metrics::Operation deleteOperation;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db},
          collection{database[phaseContext["Collection"].to<std::string>()]},
          deleteOperation{phaseContext.operation("Delete", id)} {}
};

void Deleter::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto statTracker = config->deleteOperation.start();
            /*
             * This will delete the oldest document as .find() returns documents
             * in order of _id.
             */
            const auto result = config->collection.find_one_and_delete({});
            if (result) {
                statTracker.addDocuments(1);
                statTracker.addBytes(result->view().length());
                statTracker.success();
            } else {
                statTracker.failure();
            }
        }
    }
}

Deleter::Deleter(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], Deleter::id()} {}

namespace {
auto registerDeleter = Cast::registerDefault<Deleter>();
}  // namespace
}  // namespace genny::actor
