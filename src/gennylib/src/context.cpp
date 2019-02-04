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
#include <gennylib/PoolFactory.hpp>

namespace genny {

WorkloadContext::WorkloadContext(
    YAML::Node node,
    metrics::Registry& registry,
    Orchestrator& orchestrator,
    const std::string& mongoUri,
    const Cast& cast,
    std::function<void(const mongocxx::events::command_started_event&)> apmCallback)
    : v1::ConfigNode(std::move(node)),
      _registry{&registry},
      _orchestrator{&orchestrator},
      _rateLimiters{10} {

    // This is good enough for now. Later can add a WorkloadContextValidator concept
    // and wire in a vector of those similar to how we do with the vector of Producers.
    if (this->get_noinherit<std::string>("SchemaVersion") != "2018-07-01") {
        throw InvalidConfigurationException("Invalid schema version");
    }

    // Make sure we have a valid mongocxx instance happening here
    mongocxx::instance::current();

    // TODO: make this optional and default to mongodb://localhost:27017
    auto poolFactory = PoolFactory(mongoUri, apmCallback);

    auto queryOpts =
        this->get_noinherit<std::map<std::string, std::string>, false>("Pool", "QueryOptions");
    if (queryOpts) {
        poolFactory.setOptions(PoolFactory::kQueryOption, *queryOpts);
    }

    auto accessOpts =
        this->get_noinherit<std::map<std::string, std::string>, false>("Pool", "AccessOptions");
    if (accessOpts) {
        poolFactory.setOptions(PoolFactory::kAccessOption, *accessOpts);
    }

    _clientPool = poolFactory.makePool();

    // TODO: Remove hasApmOpts when TIG-1396 is resolved.
    _hasApmOpts = bool{apmCallback};

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

mongocxx::pool::entry WorkloadContext::client(const std::string& name, int instance) {
    if (_hasApmOpts) {
        // TODO: Remove this conditional when TIG-1396 is resolved.
        return _clientPool->acquire();
    }
    auto entry = _clientPool->try_acquire();
    if (!entry) {
        throw InvalidConfigurationException("Failed to acquire an entry from the client pool.");
    }
    return std::move(*entry);
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

}  // namespace genny
