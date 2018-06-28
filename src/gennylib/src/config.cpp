#include <gennylib/config.hpp>


std::vector<std::unique_ptr<genny::ActorContext>> createActorConfigs(genny::WorkloadContext& context) {
    auto out = std::vector<std::unique_ptr<genny::ActorContext>>{};
    for (const auto& actor : context["Actors"]) {
        // need to do this over make_unique so we can take advantage of class-friendship
        out.push_back(std::unique_ptr<genny::ActorContext>{new genny::ActorContext(actor, context)});
    }
    return out;
}

void validateWorkloadConfig(genny::WorkloadContext& context) {
    context.errors().require(context, std::string("SchemaVersion"), std::string("2018-07-01"));
}

genny::WorkloadContext::ActorVector genny::WorkloadContext::constructActors(const std::vector<Producer>& producers) {
    validateWorkloadConfig(*this);

    auto actorContexts = createActorConfigs(*this);
    genny::WorkloadContext::ActorVector actors {};
    for (const auto& producer : producers)
        for (auto& actorConfig : actorContexts)
            for (auto&& actor : producer(*actorConfig.get()))
                actors.push_back(std::move(actor));
    return actors;
}

