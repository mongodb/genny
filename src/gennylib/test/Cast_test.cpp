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

#include "test.h"

#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <log.hh>

using namespace genny;

/////
// Possibly extract this section to a ContextHelper if it seems useful elsewhere
namespace genny {

inline YAML::Node createWorkloadYaml(const std::string& type, const std::string& actorYaml) {
    auto base = YAML::Load(R"(
SchemaVersion: 2018-07-01
Actors: []
)");
    auto actor = YAML::Load(std::string{actorYaml});
    actor["Type"] = type;

    base["Actors"].push_back(actor);
    return base;
}

template <class ProducerT>
class ContextHelper {

    static_assert(std::is_constructible_v<ProducerT, const std::string&>);

public:
    explicit ContextHelper(const std::string& name, const std::string& actorYaml = "")
        : _producer{std::make_shared<ProducerT>(name)},
          _registration{globalCast().registerCustom(_producer)},
          _node{createWorkloadYaml(name, actorYaml)},
          _registry{},
          _orchestratorGauge{_registry.gauge("Genny.Orchestrator")},
          _orchestrator{_orchestratorGauge},
          _workloadContext{
              _node, _registry, _orchestrator, "mongodb://localhost:27017", genny::globalCast()} {}

    void run() {
        for (auto&& actor : _workloadContext.actors()) {
            actor->run();
        }
    }

    // add other helper methods here (e.g. exposing Orchestrator etc) as-needed.

private:
    std::shared_ptr<ProducerT> _producer;
    Cast::Registration _registration;
    YAML::Node _node;
    metrics::Registry _registry;
    metrics::Gauge _orchestratorGauge;
    Orchestrator _orchestrator;
    WorkloadContext _workloadContext;
};

}  // namespace genny

// End ContextHelper
/////////////////////


std::atomic_int calls = 0;

class MyActor : public Actor {
public:
    MyActor(ActorContext& context) : Actor(context) {}
    void run() override {
        ++calls;
    }
};

class MyProducer : public ActorProducer {
public:
    MyProducer(const std::string_view& name) : ActorProducer(name) {}

    ActorVector produce(ActorContext& context) override {
        ActorVector out;
        // two!
        out.emplace_back(std::make_unique<MyActor>(context));
        out.emplace_back(std::make_unique<MyActor>(context));
        return out;
    }
};

TEST_CASE("Can register a new ActorProducer") {
    genny::ContextHelper<MyProducer> helper{"MyActor"};
    helper.run();
    REQUIRE(calls == 2);
}
