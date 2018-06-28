#include <gennylib/config.hpp>


std::vector<std::unique_ptr<genny::ActorConfig>> genny::WorkloadConfig::createActorConfigs() {
    auto out = std::vector<std::unique_ptr<genny::ActorConfig>>{};
    for (const auto& actor : _node["Actors"]) {
        // need to do this over make_unique so we can take advantage of class-friendship
        out.push_back(std::unique_ptr<genny::ActorConfig>{new ActorConfig(actor, *this)});
    }
    return out;
}

void genny::WorkloadConfig::validateWorkloadConfig() {
    _errorBag.require(_node, std::string("SchemaVersion"), std::string("2018-07-01"));
}

genny::ActorContext genny::ActorContextFactory::build(const YAML::Node& root,
                                                      genny::metrics::Registry& registry,
                                                      genny::Orchestrator& orchestrator) const {
    ActorContext out {root, registry, orchestrator};

    genny::ActorContext::ActorVector actors {};
    for (const auto& producer : _producers)
        for (auto& actorConfig : out._workloadConfig.actorConfigs())
            for (auto&& actor : producer(*actorConfig.get()))
                actors.push_back(std::move(actor));

    out._actors = std::move(actors);

    return out;
}

