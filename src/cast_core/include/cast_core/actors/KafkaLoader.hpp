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

#ifndef HEADER_0D8E1871_D41D_4AAA_83DC_1F73C9B6D110_INCLUDED
#define HEADER_0D8E1871_D41D_4AAA_83DC_1F73C9B6D110_INCLUDED

#include <memory>
#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

#include <librdkafka/rdkafka.h>
#include <librdkafka/rdkafkacpp.h>

namespace genny::actor {

/**
 * Generates documents and publishes them to the specified kafka cluster and topic.
 *
 * ```yaml
 * SchemaVersion: 2018-07-01
 * Actors:
 * - Name: KafkaLoader
 *   Type: KafkaLoader
 *   BootstrapServers: localhost:9092
 *   Topic: example-topic
 *   Phases:
 *   - Repeat: 1000
 *     Document: foo
 * ```
 *
 * Owner: "@10gen/atlas-streams"
 */
class KafkaLoader : public Actor {
public:
    explicit KafkaLoader(ActorContext& context);
    ~KafkaLoader() = default;

    void run() override;

    static std::string_view defaultName() {
        return "KafkaLoader";
    }

private:
    RdKafka::Conf* makeKafkaConfig() const;

    // Kafka bootstrap servers.
    std::string _bootstrapServers;

    // Kafka topic to publish documents to.
    std::string _topic;

    // Total number of documents inserted into the kafka topic.
    genny::metrics::Operation _inserts;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    std::unique_ptr<RdKafka::Producer> _producer;
    std::string _err;
};

}  // namespace genny::actor

#endif  // HEADER_0D8E1871_D41D_4AAA_83DC_1F73C9B6D110_INCLUDED
