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

#include <cast_core/actors/KafkaLoader.hpp>

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

namespace {

// Kafka producer flush timeout. Flush is called after each phase.
static constexpr int kKafkaFlushTimeoutMs = 10'000;

};

struct KafkaLoader::PhaseConfig {
    DocumentGenerator documentExpr;

    PhaseConfig(PhaseContext& phaseContext, ActorId id) :
        documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}
};

void KafkaLoader::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto document = config->documentExpr();
            std::string json = bsoncxx::to_json(document.view());

            auto inserts = _totalInserts.start();
            BOOST_LOG_TRIVIAL(debug) << " KafkaLoader Inserting " << json;

            RdKafka::ErrorCode err = _producer->produce(
                /* topic */         _topic,
                /* partition */     RdKafka::Topic::PARTITION_UA,
                /* flags */         RdKafka::Producer::RK_MSG_BLOCK | RdKafka::Producer::RK_MSG_COPY,
                /* payload */       const_cast<char*>(json.c_str()),
                /* len */           json.size(),
                /* key */           nullptr,
                /* key_len */       0,
                /* timestamp */     0,
                /* msg_opaque */    nullptr
            );

            if (err != RdKafka::ERR_NO_ERROR) {
                inserts.failure();
                BOOST_THROW_EXCEPTION(std::runtime_error(std::to_string(err)));
                continue;
            }

            inserts.addDocuments(1);
            inserts.addBytes(document.length());
            inserts.success();
        }

        _producer->flush(kKafkaFlushTimeoutMs);
    }
}

KafkaLoader::KafkaLoader(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", KafkaLoader::id())},
      _bootstrapServers{context["BootstrapServers"].to<std::string>()},
      _topic{context["Topic"].to<std::string>()},
      _producer{RdKafka::Producer::create(makeKafkaConfig(), _err)},
      _loop{context, KafkaLoader::id()} {
    if (!_producer) {
        BOOST_THROW_EXCEPTION(MongoException(_err));
    }
}

RdKafka::Conf* KafkaLoader::makeKafkaConfig() const {
    RdKafka::Conf* config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    std::string err;

    config->set("bootstrap.servers", _bootstrapServers, err);
    config->set("queue.buffering.max.ms", "1000", err);

    return config;
}

namespace {
auto registerKafkaLoader = Cast::registerDefault<KafkaLoader>();
}  // namespace
}  // namespace genny::actor
