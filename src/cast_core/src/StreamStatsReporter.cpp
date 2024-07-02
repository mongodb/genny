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

#include <cast_core/actors/StreamStatsReporter.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/array/view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <metrics/operation.hpp>

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

// Poll interval for `streams_getStats`.
static constexpr std::chrono::milliseconds kPollInterval{1000ms};

}; // namespace

struct StreamStatsReporter::PhaseConfig {
    mongocxx::database database;
    bsoncxx::document::value statsCommand = bsoncxx::from_json("{}");

    int64_t expectedDocumentCount{0};

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db}, expectedDocumentCount{phaseContext["ExpectedDocumentCount"].to<int64_t>()} {
        int threads = phaseContext.actor()["Threads"].to<int>();
        if (threads != 1) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("StreamStatsReporter only allows 1 thread"));
        }

        auto repeat = phaseContext["Repeat"].maybe<int>();
        if (!repeat || *repeat != 1) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("StreamStatsReporter only allows a single repeat"));
        }

        std::string tenantId = phaseContext["TenantId"].to<std::string>();
        std::string streamProcessorName = phaseContext["StreamProcessorName"].to<std::string>();
        std::string streamProcessorId = phaseContext["StreamProcessorId"].to<std::string>();
        bsoncxx::builder::stream::document builder;
        statsCommand = builder
            << "streams_getStats" << ""
            << "tenantId" << tenantId
            << "name" << streamProcessorName
            << "processorId" << streamProcessorId
            << bsoncxx::builder::stream::finalize;
    }
};

void StreamStatsReporter::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto throughput = _throughput.start();

            int64_t inputMessageCount{0};
            double inputMessageBytes{0};
            while (_orchestrator.continueRunning()) {
                auto reply = config->database.run_command(config->statsCommand.view());
                BOOST_LOG_TRIVIAL(debug) << "stats reply: " << bsoncxx::to_json(reply);

                inputMessageCount = reply.view()["inputMessageCount"].get_int64();
                inputMessageBytes = reply.view()["inputMessageSize"].get_double();
                if (inputMessageCount >= config->expectedDocumentCount) {
                    break;
                }

                _orchestrator.sleepUntilOrPhaseEnd(std::chrono::steady_clock::now() + kPollInterval, config.phaseNumber());
            }

            throughput.addDocuments(inputMessageCount);
            throughput.addIterations(inputMessageCount);
            throughput.addBytes(inputMessageBytes);
            throughput.success();
        }
    }
}

StreamStatsReporter::StreamStatsReporter(genny::ActorContext& context)
    : Actor{context},
      _throughput{context.operation("Throughput", StreamStatsReporter::id())},
      _client{std::move(context.client())},
      _loop{context, (*_client)[context["Database"].to<std::string>()], StreamStatsReporter::id()},
      _orchestrator{context.orchestrator()} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
auto registerStreamStatsReporter = Cast::registerDefault<StreamStatsReporter>();
}  // namespace
}  // namespace genny::actor
