#include <gennylib/context.hpp>
#include <memory>

// Helper method to convert Actors:[...] to ActorContexts
std::vector<std::unique_ptr<genny::ActorContext>> genny::WorkloadContext::constructActorContexts(
    WorkloadContext* workloadContext) {
    auto out = std::vector<std::unique_ptr<genny::ActorContext>>{};
    for (auto&& actor : this->get_noinherit("Actors")) {
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
        auto configuredIndex = phase["Phase"].as<genny::PhaseNumber>(index);
        auto [it, success] = out.try_emplace(
            configuredIndex, std::make_unique<genny::PhaseContext>(phase, *actorContext));
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

// Helper method to convert Operations:[...] to OperationContexts
std::unordered_map<std::string, std::unique_ptr<genny::OperationContext>>
genny::PhaseContext::constructOperationContexts(const YAML::Node& node,
                                                genny::PhaseContext* phaseContext) {
    std::unordered_map<std::string, std::unique_ptr<genny::OperationContext>> out;
    auto operations = phaseContext->get<YAML::Node, false>("Operations");
    if (!operations) {
        return out;
    }

    for (const auto& operation : *operations) {
        auto metricsName = operation["MetricsName"].as<std::string>();
        std::cout << "MetricsName: " << metricsName << std::endl;
        auto [it, success] = out.try_emplace(
            metricsName, std::make_unique<genny::OperationContext>(operation, *phaseContext));
        if (!success) {
            std::stringstream msg;
            msg << "Duplicate metrics name " << metricsName;
            throw InvalidConfigurationException(msg.str());
        }
    }
    return out;
}
