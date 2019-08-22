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

#include <cast_core/actors/RollingCollectionWriter.hpp>
#include <cast_core/actors/RollingCollectionManager.hpp>

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
struct RollingCollectionWriter::PhaseConfig {
    DocumentGenerator documentExpr;
    metrics::Operation insertOperation;
    mongocxx::database database;
    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)},
          insertOperation{phaseContext.operation("Insert", id)} {
          }
};

void RollingCollectionWriter::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto statTracker = config->insertOperation.start();
            auto document = config->documentExpr();
            auto collectionName = _rollingCollectionNames.back();
            auto collection = config->database[collectionName];
            collection.insert_one(document.view());
            statTracker.addDocuments(1);
            statTracker.addBytes(document.view().length());
            statTracker.success();
        }
    }
}

RollingCollectionWriter::RollingCollectionWriter(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _rollingCollectionNames{WorkloadContext::getActorSharedState<RollingCollectionManager,
        RollingCollectionManager::RollingCollectionNames>()},
      _loop{context,
            (*_client)[context["Database"].to<std::string>()],
            RollingCollectionWriter::id()} {
        }

namespace {
auto registerRollingCollectionWriter = Cast::registerDefault<RollingCollectionWriter>();
}  // namespace
}  // namespace genny::actor
