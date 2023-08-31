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

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/KafkaTestFixture.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

//
// ⚠️ There is a "known" failure that you should find and fix as a bit of
// an exercise in reading and testing your Actor. ⚠️
//

TEST_CASE_METHOD(KafkaTestFixture, "KafkaLoader successfully connects to a Kafka broker.", "[standalone][KafkaLoader]") {
    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Clients:
          Default:
            URI: )" + KafkaTestFixture::connectionUri() + R"(
        Actors:
        - Name: KafkaLoader
          Type: KafkaLoader
          BootstrapServers: localhost:9092
          Topic: topic-in
          Phases:
          - Repeat: 1
            Document: {foo: {^RandomInt: {min: 0, max: 100}}}
    )", __FILE__);


    SECTION("Inserts documents into the kafka broker.") {
        try {
            genny::ActorHelper ah(nodes.root(), 1);
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
}  // namespace
}  // namespace genny
