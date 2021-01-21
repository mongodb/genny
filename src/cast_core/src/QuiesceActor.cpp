// Copyright 2021-present MongoDB Inc.
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

#include <cast_core/actors/QuiesceActor.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>
#include <gennylib/quiesce.hpp>

namespace genny::actor {
std::string getDbName(const PhaseContext& phaseContext) {
    auto phaseDb = phaseContext["Database"].maybe<std::string>();
    auto actorDb = phaseContext.actor()["Database"].maybe<std::string>();
    if (!phaseDb && !actorDb) {
        BOOST_THROW_EXCEPTION(
            InvalidConfigurationException("Must give Database in Phase or Actor block."));
    }
    return phaseDb ? *phaseDb : *actorDb;
}

struct QuiesceActor::PhaseConfig {
    PhaseConfig(PhaseContext& context) {
        auto actorDb = context.actor()["Database"].maybe<std::string>();
        if (!actorDb) {
            BOOST_THROW_EXCEPTION(
                InvalidConfigurationException("Must give Database in Phase or Actor block."));
        }
        dbName = *actorDb;
    }

    std::string dbName;
};


void QuiesceActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto quiesceContext = _totalQuiesces.start();
            BOOST_LOG_TRIVIAL(debug) << "QuiesceActor quiescing cluster.";
            if (quiesce(_client, config->dbName)) {
                quiesceContext.success();
            } else {
                quiesceContext.failure();
            }
        }
    }
}

QuiesceActor::QuiesceActor(genny::ActorContext& context)
    : Actor{context},
      _totalQuiesces{context.operation("Quiesce", QuiesceActor::id())},
      _client{context.client()},
      _loop{context} {}

namespace {
auto registerQuiesceActor = Cast::registerDefault<QuiesceActor>();
}  // namespace
}  // namespace genny::actor
