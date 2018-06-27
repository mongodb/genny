#include <gennylib/config.hpp>


std::vector<std::unique_ptr<const genny::ActorConfig>> genny::WorkloadConfig::createActorConfigs(
    const YAML::Node& node, const genny::WorkloadConfig& workloadConfig) {
    auto out = std::vector<std::unique_ptr<const genny::ActorConfig>>{};
    for (const auto& actor : node["Actors"]) {
        // need to do this over make_unique so we can take advantage of class-friendship
        out.push_back(std::unique_ptr<const genny::ActorConfig>{new ActorConfig(actor, workloadConfig)});
    }
    return out;
}

void genny::WorkloadConfig::validateWorkloadConfig() {
    _errorBag->require("SchemaVersion",
        get("SchemaVersion").as<std::string>(),
        "2018-06-27"
    );
}

genny::PhasedActorFactory::ActorVector genny::PhasedActorFactory::actors(genny::ErrorBag* const errorBag) const {
    auto out = ActorVector{};
    for (const auto& producer : _producers)
        for (const auto& actorConfig : _workloadConfig.actorConfigs())
            for (auto&& actor : producer(actorConfig.get(), &_workloadConfig, errorBag))
                out.push_back(std::move(actor));
    return out;
}

genny::PhasedActorFactory::PhasedActorFactory(const YAML::Node& root,
                                              genny::metrics::Registry& registry,
                                              genny::Orchestrator& orchestrator,
                                              genny::ErrorBag& errorBag)
    : _workloadConfig{root, registry, orchestrator, errorBag} {}
