#include <gennylib/context.hpp>
#include <memory>

// Helper method to convert Actors:[...] to ActorContexts
std::vector<std::unique_ptr<genny::ActorContext>> genny::WorkloadContext::constructActorContexts() {
    auto out = std::vector<std::unique_ptr<genny::ActorContext>>{};
    for (const auto& actor : _node["Actors"]) {
        out.emplace_back(std::make_unique<genny::ActorContext>(actor, *this));
    }
    return out;
}


genny::WorkloadContext::ActorVector genny::WorkloadContext::constructActors(const std::vector<Producer>& producers) {

    _errors.require(*this, std::string("SchemaVersion"), std::string("2018-07-01"));

    genny::WorkloadContext::ActorVector actors {};
    for (const auto& producer : producers)
        for (auto& actorContext : _actorContexts)
            for (auto&& actor : producer(*actorContext))
                actors.push_back(std::move(actor));
    return actors;
}

