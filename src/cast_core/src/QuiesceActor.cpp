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

struct QuiesceActor::PhaseConfig {
    PhaseConfig(PhaseContext& context)
        : dbName{context.actor()["Database"].to<std::string>()},
          sleepContext{context.getSleepContext()} {
        auto threads = context.actor()["Threads"].to<int>();
        if (threads > 1) {
            std::stringstream ss;
            ss << "QuiesceActor only allows 1 thread, but found " << threads << " threads.";
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(ss.str()));
        }
    }
    std::string dbName;
    SleepContext sleepContext;
};

void QuiesceActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto quiesceContext = _totalQuiesces.start();
            BOOST_LOG_TRIVIAL(debug) << "QuiesceActor quiescing cluster.";
            if (quiesce(_client, config->dbName, config->sleepContext)) {
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
      _client{context.client(v1::DEFAULT_CLIENT_NAME, QuiesceActor::id())},
      _loop{context} {}

namespace {
auto registerQuiesceActor = Cast::registerDefault<QuiesceActor>();
}  // namespace
}  // namespace genny::actor
