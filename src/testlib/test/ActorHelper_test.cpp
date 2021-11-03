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

#include <memory.h>

#include <yaml-cpp/yaml.h>

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/context.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

using Catch::Matchers::Matches;

TEST_CASE("Actor Helper") {
    class DummyActor : public genny::Actor {
    public:
        DummyActor(genny::ActorContext& ac, genny::ActorId id) : Actor(ac, id){};

        void run() override {
            BOOST_LOG_TRIVIAL(info) << "In the run method of DummyActor";
        };

        static std::string_view defaultName() {
            return "DummyActor";
        }
    };

    SECTION("Barfs on invalid thread count (aka token)") {
        genny::NodeSource config(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: DummyActor
)",
                                 "");
        auto dummyProducer =
            std::make_shared<genny::DefaultActorProducer<DummyActor>>("DummyActor");

        auto test = [&]() {
            genny::ActorHelper ah(config.root(), -1, {{"DummyActor", dummyProducer}});
        };
        REQUIRE_THROWS_WITH(test(), Matches("Must add a positive number of tokens"));
    }

    class CtorThrowingActor : public DummyActor {
    public:
        CtorThrowingActor(genny::ActorContext& ac, genny::ActorId id) : DummyActor(ac, id) {
            throw genny::InvalidConfigurationException("CTOR Barf");
        };
    };

    SECTION("Barfs if Actor Ctor barfs") {
        genny::NodeSource config(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: DummyActor
)",
                                 "");
        auto dummyProducer =
            std::make_shared<genny::DefaultActorProducer<CtorThrowingActor>>("DummyActor");

        auto test = [&]() {
            genny::ActorHelper ah(config.root(), 1, {{"DummyActor", dummyProducer}});
        };
        REQUIRE_THROWS_WITH(test(), Matches("CTOR Barf"));
    }

    SECTION("Barfs if Actor runAndVerify() barfs") {
        genny::NodeSource config(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: DummyActor
)",
                                 "");
        auto dummyProducer =
            std::make_shared<genny::DefaultActorProducer<DummyActor>>("DummyActor");

        genny::ActorHelper ah(config.root(), 1, {{"DummyActor", dummyProducer}});
        auto runFunc = [](const genny::WorkloadContext& wc) {};
        auto verifyFunc = [](const genny::WorkloadContext& wc) {
            throw genny::InvalidConfigurationException("RUN Barf");
        };
        REQUIRE_THROWS_WITH(ah.runAndVerify(runFunc, verifyFunc), Matches("RUN Barf"));
    }
}
