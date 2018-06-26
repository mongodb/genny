#include <gennylib/config.hpp>


std::vector<std::unique_ptr<const genny::ActorConfig>> genny::WorkloadConfig::createActorConfigs(
    const YAML::Node& node, genny::WorkloadConfig& workloadConfig) {
    auto out = std::vector<std::unique_ptr<const genny::ActorConfig>>{};
    for (const auto& actor : node["Actors"]) {
        out.push_back(std::unique_ptr<const genny::ActorConfig>{new ActorConfig(actor, workloadConfig)});
    }
    return out;
}

genny::PhasedActorFactory::ActorVector genny::PhasedActorFactory::actors() const {
    auto out = ActorVector{};
    for (const auto& producer : _producers) {
        for (const auto& actorConfig : _workloadConfig.actorConfigs()) {
            const WorkloadConfig* workloadConfig = &_workloadConfig;
            ActorVector produced = producer(actorConfig.get(), workloadConfig);
            for (auto&& actor : produced) {
                out.push_back(std::move(actor));
            }
        }
    }
    return out;
}

genny::PhasedActorFactory::PhasedActorFactory(const YAML::Node& root,
                                              genny::metrics::Registry& registry,
                                              genny::Orchestrator& orchestrator)
    : _workloadConfig{root, registry, orchestrator} {}
