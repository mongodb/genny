#include <utils.hpp>

#include <gennylib/context.hpp>

void genny::run_actor_helper(const YAML::Node& config, int token_count, const Cast& cast) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    orchestrator.addRequiredTokens(token_count);

    metrics::Registry registry;
    WorkloadContext wl(config, registry, orchestrator, "mongodb://localhost:27017", cast);


    std::vector<std::thread> threads;
    std::transform(cbegin(wl.actors()),
                   cend(wl.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) { return std::thread{[&]() { actor->run(); }}; });

    for (auto& thread : threads)
        thread.join();
}
