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

#include <gennylib/context.hpp>

#include <memory>
#include <sstream>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/Cast.hpp>

namespace genny {

WorkloadContext::WorkloadContext(const YAML::Node& node,
                                 metrics::Registry& registry,
                                 Orchestrator& orchestrator,
                                 const std::string& mongoUri,
                                 const Cast& cast,
                                 PoolManager::OnCommandStartCallback apmCallback)
    : v1::ConfigNode(node),
      _registry{&registry},
      _orchestrator{&orchestrator},
      _rateLimiters{10},
      _poolManager{mongoUri, apmCallback} {

    // This is good enough for now. Later can add a WorkloadContextValidator concept
    // and wire in a vector of those similar to how we do with the vector of Producers.
    if (this->get_noinherit<std::string>("SchemaVersion") != "2018-07-01") {
        throw InvalidConfigurationException("Invalid schema version");
    }

    // Make sure we have a valid mongocxx instance happening here
    mongocxx::instance::current();

    // Make a bunch of actor contexts
    for (const auto& actor : this->get_noinherit("Actors")) {
        _actorContexts.emplace_back(std::make_unique<genny::ActorContext>(actor, *this));
    }

    // Default value selected from random.org, by selecting 2 random numbers
    // between 1 and 10^9 and concatenating.
    _rng.seed(this->get_noinherit<int, false>("RandomSeed").value_or(269849313357703264));

    for (auto& actorContext : _actorContexts) {
        for (auto&& actor : _constructActors(cast, actorContext)) {
            _actors.push_back(std::move(actor));
        }
    }
    _done = true;
}

ActorVector WorkloadContext::_constructActors(const Cast& cast,
                                              const std::unique_ptr<ActorContext>& actorContext) {
    auto actors = ActorVector{};
    auto name = actorContext->get<std::string>("Type");

    std::shared_ptr<ActorProducer> producer;
    try {
        producer = cast.getProducer(name);
    } catch (const std::out_of_range&) {
        std::ostringstream stream;
        stream << "Unable to construct actors: No producer for '" << name << "'." << std::endl;
        cast.streamProducersTo(stream);
        throw InvalidConfigurationException(stream.str());
    }

    for (auto&& actor : producer->produce(*actorContext)) {
        actors.emplace_back(std::forward<std::unique_ptr<Actor>>(actor));
    }
    return actors;
}

mongocxx::pool::entry WorkloadContext::client(const std::string& name, size_t instance) {
    return _poolManager.client(name, instance, *this);
}

v1::GlobalRateLimiter* WorkloadContext::getRateLimiter(const std::string& name,
                                                       const RateSpec& spec) {
    if (_rateLimiters.count(name) == 0) {
        _rateLimiters.emplace(std::make_pair(name, std::make_unique<v1::GlobalRateLimiter>(spec)));
    }
    auto rl = _rateLimiters[name].get();
    rl->addUser();
    return rl;
}

// Helper method to convert Phases:[...] to PhaseContexts
std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> ActorContext::constructPhaseContexts(
    const YAML::Node&, ActorContext* actorContext) {
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> out;
    auto phases = actorContext->get<YAML::Node, false>("Phases");
    if (!phases) {
        return out;
    }

    int index = 0;
    for (const auto& phase : *phases) {
        // If we don't have a node or we are a null type, then we are a NoOp
        if (!phase || phase.IsNull()) {
            std::ostringstream ss;
            ss << "Encountered a null/empty phase. "
                  "Every phase should have at least be an empty map.";
            throw InvalidConfigurationException(ss.str());
        }

        auto configuredIndex = phase["Phase"].as<PhaseNumber>(index);
        auto [it, success] =
            out.try_emplace(configuredIndex, std::make_unique<PhaseContext>(phase, *actorContext));
        if (!success) {
            std::stringstream msg;
            msg << "Duplicate phase " << configuredIndex;
            throw InvalidConfigurationException(msg.str());
        }
        ++index;
    }
    actorContext->orchestrator().phasesAtLeastTo(out.size() - 1);
    return out;
}

// this could probably be made into a free-function rather than an
// instance method
bool PhaseContext::_isNop() const {
    auto hasNoOp = get<bool, false>("Nop").value_or(false)  //
        || get<bool, false>("nop").value_or(false)          //
        || get<bool, false>("NoOp").value_or(false)         //
        || get<bool, false>("noop").value_or(false);

    // If we had the simple Nop key, just exit out now
    if (hasNoOp)
        return true;

    // If we don't have an operation or our operation isn't a map, then we're not a NoOp
    auto maybeOperation = get<YAML::Node, false>("Operation");
    if (!maybeOperation)
        return false;

    // If we have a simple string, use that
    // If we have a full object, get "OperationName"
    // Otherwise, we're null
    auto yamlOpName = YAML::Node{};
    if (maybeOperation->IsScalar())
        yamlOpName = *maybeOperation;
    else if (maybeOperation->IsMap())
        yamlOpName = (*maybeOperation)["OperationName"];

    // At this stage, we should have a string scalar
    if (!yamlOpName.IsScalar())
        return false;

    // Fall back to an empty string in case we cannot convert to string
    const auto& opName = yamlOpName.as<std::string>("");
    return (opName == "Nop")   //
        || (opName == "nop")   //
        || (opName == "NoOp")  //
        || (opName == "noop");
}

bool PhaseContext::isNop() const {
    auto isNop = _isNop();

    // Check to make sure we haven't broken our rules
    if (isNop && _node.size() > 1) {
        if (_node.size() != 2 || !_node["Phase"]) {
            throw InvalidConfigurationException(
                "'Nop' cannot be used with any other keywords except 'Phase'. Check YML "
                "configuration.");
        }
    }

    return isNop;
}

}  // namespace genny
