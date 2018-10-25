#include <gennylib/context.hpp>

#include <memory>
#include <sstream>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/Cast.hpp>

namespace genny {

WorkloadContext::WorkloadContext(YAML::Node node,
                                 metrics::Registry& registry,
                                 Orchestrator& orchestrator,
                                 const std::string& mongoUri,
                                 const Cast& cast)
    : _node{std::move(node)}, _registry{&registry}, _orchestrator{&orchestrator} {
    // This is good enough for now. Later can add a WorkloadContextValidator concept
    // and wire in a vector of those similar to how we do with the vector of Producers.
    if (get_static<std::string>(_node, "SchemaVersion") != "2018-07-01") {
        throw InvalidConfigurationException("Invalid schema version");
    }

    // Make sure we have a valid mongocxx instance happening here
    mongocxx::instance::current();

    // TODO: make this optional and default to mongodb://localhost:27017
    _clientPool = std::make_unique<mongocxx::pool>(mongocxx::uri{mongoUri});

    // Make a bunch of actor contexts
    for (const auto& actor : get_static(node, "Actors")) {
        _actorContexts.emplace_back(std::make_unique<genny::ActorContext>(actor, *this));
    }

    // Default value selected from random.org, by selecting 2 random numbers
    // between 1 and 10^9 and concatenating.
    _rng.seed(get_static<int, false>(node, "RandomSeed").value_or(269849313357703264));

    for (auto& actorContext : _actorContexts) {
        for (auto&& actor : _constructActors(cast, actorContext)) {
            _actors.push_back(std::move(actor));
        }
    }
    _done = true;
}

ActorVector WorkloadContext::_constructActors(const Cast& cast,
                                              const std::unique_ptr<ActorContext> &actorContext) {
    auto actors = ActorVector{};
    auto name = actorContext->get<std::string>("Type");
    auto producer = cast.getProducer(name);
    if (!producer) {
        std::ostringstream stream;
        stream << "Unable to construct actors: No producer for '" << name << "'.";
        throw std::out_of_range(stream.str());
    }

    auto threads = actorContext->get<int, /* Required = */ false>("Threads").value_or(1);
    for (decltype(threads) i = 0; i < threads; ++i) {
        for (auto&& actor : producer->produce(*actorContext)) {
            actors.emplace_back(std::forward<std::unique_ptr<Actor>>(actor));
        }
    }
    return actors;
}

// Helper method to convert Phases:[...] to PhaseContexts
std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>>
ActorContext::constructPhaseContexts(const YAML::Node&, ActorContext* actorContext) {
    std::unordered_map<PhaseNumber, std::unique_ptr<PhaseContext>> out;
    auto phases = actorContext->get<YAML::Node, false>("Phases");
    if (!phases) {
        return out;
    }

    int index = 0;
    for (const auto& phase : *phases) {
        auto configuredIndex = phase["Phase"].as<PhaseNumber>(index);
        auto [it, success] = out.try_emplace(
            configuredIndex, std::make_unique<PhaseContext>(phase, *actorContext));
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

mongocxx::pool::entry ActorContext::client() {
    auto entry = _workload->_clientPool->try_acquire();
    if (!entry) {
        throw InvalidConfigurationException("Failed to acquire an entry from the client pool.");
    }
    return std::move(*entry);
}

} // namespace genny
