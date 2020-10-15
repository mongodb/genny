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
#include <set>
#include <sstream>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/Cast.hpp>
#include <metrics/metrics.hpp>

namespace genny {

WorkloadContext::WorkloadContext(const Node& node,
                                 Orchestrator& orchestrator,
                                 const std::string& mongoUri,
                                 const Cast& cast,
                                 v1::PoolManager::OnCommandStartCallback apmCallback)
    : v1::HasNode{node},
      _orchestrator{&orchestrator},
      _rateLimiters{10},
      _poolManager{mongoUri, apmCallback} {

    std::set<std::string> validSchemaVersions{"2018-07-01"};

    // This is good enough for now. Later can add a WorkloadContextValidator concept
    // and wire in a vector of those similar to how we do with the vector of Producers.
    if (const std::string schemaVersion = (*this)["SchemaVersion"].to<std::string>();
        validSchemaVersions.count(schemaVersion) != 1) {
        std::ostringstream errMsg;
        errMsg << "Invalid Schema Version: " << schemaVersion;
        throw InvalidConfigurationException(errMsg.str());
    }

    // Make sure we have a valid mongocxx instance happening here
    mongocxx::instance::current();

    // Set the metrics format information.
    auto format = ((*this)["Metrics"]["Format"])
                      .maybe<metrics::MetricsFormat>()
                      .value_or(metrics::MetricsFormat("cedar-csv"));
    auto metricsPath =
        ((*this)["Metrics"]["Path"]).maybe<std::string>().value_or("build/genny-metrics");
    _registry = genny::metrics::Registry(std::move(format), std::move(metricsPath));

    // Make a bunch of actor contexts
    for (const auto& [k, actor] : (*this)["Actors"]) {
        _actorContexts.emplace_back(std::make_unique<genny::ActorContext>(actor, *this));
    }

    // Default value selected from random.org, by selecting 2 random numbers
    // between 1 and 10^9 and concatenating.
    _rng.seed((*this)["RandomSeed"].maybe<long>().value_or(269849313357703264));

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
    auto name = (*actorContext)["Type"].to<std::string>();

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
    return _poolManager.client(name, instance, this->_node);
}

v1::GlobalRateLimiter* WorkloadContext::getRateLimiter(const std::string& name,
                                                       const RateSpec& spec) {
    if (this->isDone()) {
        BOOST_THROW_EXCEPTION(std::logic_error("Cannot create rate-limiters after setup"));
    }
    if (_rateLimiters.count(name) == 0) {
        _rateLimiters.emplace(std::make_pair(name, std::make_unique<v1::GlobalRateLimiter>(spec)));
    }
    auto rl = _rateLimiters[name].get();
    rl->addUser();

    // Reset the rate-limiter at the start of every Phase
    this->_orchestrator->addPrePhaseStartHook(
        [rl](const Orchestrator*) { rl->resetLastEmptied(); });
    return rl;
}

v1::GlobalRateLimiter* WorkloadContext::getRateLimiter(const std::string& name,
                                                       const PercentileRateSpec& spec) {
    if (this->isDone()) {
        BOOST_THROW_EXCEPTION(std::logic_error("Cannot create rate-limiters after setup"));
    }
    if (_rateLimiters.count(name) == 0) {
        _rateLimiters.emplace(std::make_pair(name, std::make_unique<v1::GlobalRateLimiter>(spec)));
    }
    auto rl = _rateLimiters[name].get();
    rl->addUser();

    // Reset the rate-limiter at the start of every Phase
    this->_orchestrator->addPrePhaseStartHook(
        [rl](const Orchestrator*) { rl->resetLastEmptied(); });
    return rl;
}

DefaultRandom& WorkloadContext::getRNGForThread(ActorId id) {
    if (this->isDone()) {
        BOOST_THROW_EXCEPTION(std::logic_error("Cannot create RNGs after setup"));
    }
    if (auto rng = _rngRegistry.find(id); rng == _rngRegistry.end()) {
        auto [it, success] = _rngRegistry.try_emplace(id, _rng());
        if (!success) {
            // This should be impossible.
            // But invariants don't hurt we only call this during setup
            throw std::logic_error("Already have DefaultRandom for Actor " + std::to_string(id));
        }
    }
    return _rngRegistry[id];
}

// Helper method to convert Phases:[...] to PhaseContexts
std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> ActorContext::constructPhaseContexts(
    const Node&, ActorContext* actorContext) {
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> out;
    auto& phases = (*actorContext)["Phases"];
    if (!phases) {
        return out;
    }
    PhaseNumber lastPhaseNumber = 0;
    for (const auto& [k, phase] : phases) {
        // If we don't have a node or we are a null type, then we are a Nop
        if (!phase || phase.isNull()) {
            std::ostringstream ss;
            ss << "Encountered a null/empty phase. "
                  "Every phase should have at least be an empty map.";
            throw InvalidConfigurationException(ss.str());
        }
        PhaseRangeSpec configuredRange = phase["Phase"].maybe<PhaseRangeSpec>().value_or(
            PhaseRangeSpec{IntegerSpec{lastPhaseNumber}});
        for (PhaseNumber rangeIndex = configuredRange.start; rangeIndex <= configuredRange.end;
             rangeIndex++) {
            auto [it, success] = out.try_emplace(
                rangeIndex, std::make_unique<PhaseContext>(phase, lastPhaseNumber, *actorContext));
            if (!success) {
                std::stringstream msg;
                msg << "Duplicate phase " << rangeIndex;
                throw InvalidConfigurationException(msg.str());
            }
            lastPhaseNumber++;
        }
    }
    actorContext->orchestrator().phasesAtLeastTo(out.size() - 1);
    return out;
}

bool PhaseContext::isNop() const {
    auto& nop = (*this)["Nop"];
    return nop.maybe<bool>().value_or(false);
}
}  // namespace genny
