#include <gennylib/context.hpp>
#include <memory>

// Helper method to convert Actors:[...] to ActorContexts
std::vector<std::unique_ptr<genny::ActorContext>> genny::WorkloadContext::constructActorContexts(
    const YAML::Node& node, WorkloadContext* workloadContext) {
    auto out = std::vector<std::unique_ptr<genny::ActorContext>>{};
    for (const auto& actor : get_static(node, "Actors")) {
        out.emplace_back(std::make_unique<genny::ActorContext>(actor, *workloadContext));
    }
    return out;
}

genny::ActorVector genny::WorkloadContext::constructActors(
    const std::vector<ActorProducer>& producers,
    const std::vector<std::unique_ptr<ActorContext>>& actorContexts) {
    auto actors = genny::ActorVector{};
    for (const auto& producer : producers)
        for (auto& actorContext : actorContexts)
            for (auto&& actor : producer(*actorContext))
                actors.push_back(std::move(actor));
    return actors;
}

// Helper method to convert Phases:[...] to PhaseContexts
std::unordered_map<genny::PhaseNumber, std::unique_ptr<genny::PhaseContext>>
genny::ActorContext::constructPhaseContexts(const YAML::Node&, genny::ActorContext* actorContext) {
    std::unordered_map<genny::PhaseNumber, std::unique_ptr<genny::PhaseContext>> out;
    auto phases = actorContext->get<YAML::Node, false>("Phases");
    if (!phases) {
        return out;
    }

    int index = 0;
    for (const auto& phase : *phases) {
        out.emplace(phase["Phase"].as<genny::PhaseNumber>(index),
                    std::make_unique<genny::PhaseContext>(phase, *actorContext));
        ++index;
    }
    return out;
}
