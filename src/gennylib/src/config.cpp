#include <gennylib/config.hpp>


std::vector<std::unique_ptr<genny::ActorContext>> genny::WorkloadConfig::createActorConfigs() {
    auto out = std::vector<std::unique_ptr<genny::ActorContext>>{};
    for (const auto& actor : _node["Actors"]) {
        // need to do this over make_unique so we can take advantage of class-friendship
        out.push_back(std::unique_ptr<genny::ActorContext>{new ActorContext(actor, *this)});
    }
    return out;
}

void genny::WorkloadConfig::validateWorkloadConfig() {
    _errorBag.require(_node, std::string("SchemaVersion"), std::string("2018-07-01"));
}

genny::WorkloadContext genny::WorkloadContextFactory::build(const YAML::Node& root,
                                                      genny::metrics::Registry& registry,
                                                      genny::Orchestrator& orchestrator) const {
    WorkloadContext out {root, registry, orchestrator};

    genny::WorkloadContext::ActorVector actors {};
    for (const auto& producer : _producers)
        for (auto& actorConfig : out._workloadConfig.actorContexts())
            for (auto&& actor : producer(*actorConfig.get()))
                actors.push_back(std::move(actor));

    out._actors = std::move(actors);

    return out;
}

