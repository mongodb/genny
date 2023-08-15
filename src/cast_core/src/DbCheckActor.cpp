// Copyright 2023-present MongoDB Inc.
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

#include <cast_core/actors/DbCheckActor.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/dbcheck.hpp>


namespace genny::actor {

struct DbCheckActor::PhaseConfig {
    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db}, 
          collectionName{phaseContext["Collection"].to<std::string>()},
          validateMode{phaseContext["ValidateMode"].maybe<std::string>()}  {
        auto threads = phaseContext.actor()["Threads"].to<int>();
        if (threads > 1) {
            std::stringstream ss;
            ss << "DbCheckActor only allows 1 thread, but found " << threads << " threads.";
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(ss.str()));
        }
    }

    mongocxx::database database;
    std::string collectionName;
    std::optional<std::string> validateMode;
};

void DbCheckActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto dbcheckCtx = _dbcheckMetric.start();
            auto param = config->validateMode? 
                bsoncxx::builder::stream::document() << "validateMode"
                << config->validateMode.value() << bsoncxx::builder::stream::finalize
                : bsoncxx::builder::stream::document() << bsoncxx::builder::stream::finalize;

            BOOST_LOG_TRIVIAL(debug)
                << " DbCheckActor with paramters " << bsoncxx::to_json(param.view());

            BOOST_LOG_TRIVIAL(debug) << "DbCheckActor starting dbcheck.";

            if (dbcheck(_client, config->database, config->collectionName, param)) {
                dbcheckCtx.success();
            } else {
                dbcheckCtx.failure();
            }
        }
    }
}

DbCheckActor::DbCheckActor(genny::ActorContext& context)
    : Actor{context},
      _dbcheckMetric{context.operation("DbCheck", DbCheckActor::id())},
      _client{context.client()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], DbCheckActor::id()} {}

namespace {
auto registerDbCheckActor = Cast::registerDefault<DbCheckActor>();
}  // namespace
}  // namespace genny::actor
