#include <gennylib/config.hpp>


std::vector<std::unique_ptr<genny::ActorConfig>> genny::WorkloadConfig::createActorConfigs(
    const YAML::Node& node, genny::WorkloadConfig& workloadConfig) {
    auto out = std::vector<std::unique_ptr<genny::ActorConfig>>{};
    for (const auto& actor : node["Actors"]) {
        // need to do this over make_unique so we can take advantage of class-friendship
        out.push_back(
            std::unique_ptr<genny::ActorConfig>{new ActorConfig(actor, workloadConfig)});
    }
    return out;
}

void genny::WorkloadConfig::validateWorkloadConfig() {
    _errorBag.require("SchemaVersion", _node, std::string("2018-07-01"));
}

genny::PhasedActorFactory::Results genny::PhasedActorFactory::actors() const {
    auto out = ActorVector{};
    for (const auto& producer : _producers)
        for (auto& actorConfig : _workloadConfig.actorConfigs())
            for (auto&& actor : producer(*actorConfig.get()))
                out.push_back(std::move(actor));
    return {std::move(out), _workloadConfig._errorBag};
}

genny::PhasedActorFactory::PhasedActorFactory(const YAML::Node& root,
                                              genny::metrics::Registry& registry,
                                              genny::Orchestrator& orchestrator)
    : _workloadConfig{root, registry, orchestrator} {}
