#include <gennylib/config.hpp>



std::vector<std::unique_ptr<genny::ActorConfig>>
genny::WorkloadConfig::createActorConfigs(const YAML::Node& node, genny::WorkloadConfig& workloadConfig) {
    auto out = std::vector<std::unique_ptr<genny::ActorConfig>> {};
    for(const auto& actor : node["Actors"]) {
        out.push_back(std::make_unique<ActorConfig>(actor, workloadConfig));
    }
    return out;
}