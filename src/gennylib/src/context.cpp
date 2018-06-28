#include <gennylib/context.hpp>
#include <memory>

namespace {

// Helper method to convert Actors:[...] to ActorContexts
std::vector<genny::ActorContext> createActorConfigs(genny::WorkloadContext& context) {
    auto out = std::vector<genny::ActorContext>{};
    for (const auto& actor : context["Actors"]) {
        out.emplace_back(actor, context);
    }
    return out;
}

}  // namespace


genny::WorkloadContext::ActorVector genny::WorkloadContext::constructActors(const std::vector<Producer>& producers) {

    _errors.require(*this, std::string("SchemaVersion"), std::string("2018-07-01"));

    auto actorContexts = createActorConfigs(*this);
    genny::WorkloadContext::ActorVector actors {};
    for (const auto& producer : producers)
        for (auto& actorContext : actorContexts)
            for (auto&& actor : producer(actorContext))
                actors.push_back(std::move(actor));
    return actors;
}

