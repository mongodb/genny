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

#include <cast_core/actors/CrudActor.hpp>
#include <crud_operations/OptionsConversion.hpp>

#include <chrono>
#include <memory>
#include <utility>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/json.hpp>
#include <bsoncxx/string/to_string.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>

using BsonView = bsoncxx::document::view;
using CrudActor = genny::actor::CrudActor;
using bsoncxx::type;

namespace genny::actor {

using namespace genny::crud_operations;

struct CrudActor::CollectionName {
    std::optional<std::string> collectionName;
    std::optional<int64_t> numCollections;
    explicit CollectionName(PhaseContext& phaseContext)
        : collectionName{phaseContext["Collection"].maybe<std::string>()},
          numCollections{phaseContext["CollectionCount"].maybe<IntegerSpec>()} {
        if (collectionName && numCollections) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "Collection or CollectionCount, not both in Crud Actor."));
        }
        if (!collectionName && !numCollections) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "One of Collection or CollectionCount must be provided in Crud Actor."));
        }
    }
    // Get the assigned collection name or generate a name based on collectionCount and the
    // the actorId.
    std::string generateName(ActorId id) {
        if (collectionName) {
            return collectionName.value();
        }
        auto collectionNumber = id % numCollections.value();
        return "Collection" + std::to_string(collectionNumber);
    }
};

struct CrudActor::PhaseConfig {
    std::vector<std::unique_ptr<BaseOperation>> operations;
    metrics::Operation metrics;
    std::string dbName;
    CrudActor::CollectionName collectionName;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::pool::entry& client, ActorId id)
        : dbName{getDbName(phaseContext)},
          metrics{phaseContext.actor().operation("Crud", id)},
          collectionName{phaseContext} {
        auto name = collectionName.generateName(id);
        auto addOperation = [&](const Node& node) -> std::unique_ptr<BaseOperation> {
            auto collection = (*client)[dbName][name];
            auto& yamlCommand = node["OperationCommand"];
            auto opName = node["OperationName"].to<std::string>();
            auto onSession = yamlCommand["OnSession"].maybe<bool>().value_or(false);

            auto opConstructors = getOpConstructors();
            // Grab the appropriate Operation struct defined by 'OperationName'.
            auto op = opConstructors.find(opName);
            if (op == opConstructors.end()) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "Operation '" + opName + "' not supported in Crud Actor."));
            }

            // If the yaml specified MetricsName in the Phase block, then associate the metrics
            // with the Phase; otherwise, associate with the Actor (e.g. all bulkWrite
            // operations get recorded together across all Phases). The latter case (not
            // specifying MetricsName) is legacy configuration-syntax.
            //
            // Node is convertible to bool but only explicitly so need to do the odd-looking
            // `? true : false` thing.
            const bool perPhaseMetrics = phaseContext["MetricsName"] ? true : false;

            auto createOperation = op->second;
            return createOperation(yamlCommand,
                                   onSession,
                                   collection,
                                   perPhaseMetrics ? phaseContext.operation(opName, id)
                                                   : phaseContext.actor().operation(opName, id),
                                   phaseContext,
                                   id);
        };

        operations = phaseContext.getPlural<std::unique_ptr<BaseOperation>>(
            "Operation", "Operations", addOperation);
    }
};

void CrudActor::run() {
    for (auto&& config : _loop) {
        auto session = _client->start_session();
        for (const auto&& _ : config) {
            auto metricsContext = config->metrics.start();

            for (auto&& op : config->operations) {
                op->run(session);
            }

            metricsContext.success();
        }
    }
}

CrudActor::CrudActor(genny::ActorContext& context)
    : Actor(context),
      _client{std::move(
          context.client(context.get("ClientName").maybe<std::string>().value_or("Default")))},
      _loop{context, _client, CrudActor::id()} {}

namespace {
auto registerCrudActor = Cast::registerDefault<CrudActor>();
}
}  // namespace genny::actor
