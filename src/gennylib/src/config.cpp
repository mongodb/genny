#include <gennylib/config.hpp>



std::vector<std::unique_ptr<genny::ActorConfig>>
genny::WorkloadConfig::createActorConfigs(const YAML::Node& node, genny::WorkloadConfig& workloadConfig) {
    auto out = std::vector<std::unique_ptr<genny::ActorConfig>> {};
    for(const auto& actor : node["Actors"]) {
        out.push_back(std::make_unique<ActorConfig>(actor, workloadConfig));
    }
    return out;
}

genny::PhasedActorFactory::ActorList genny::PhasedActorFactory::actors(genny::WorkloadConfig *const workloadConfig) const {
    auto out = ActorList {};
    for(const auto& producer : _producers) {
        for(const auto& actorConfig : workloadConfig->actorConfigs()) {
            ActorList produced = producer(actorConfig.get(), workloadConfig);
            for (auto&& actor : produced) {
                out.push_back(std::move(actor));
            }
        }
    }
    return out;
}
