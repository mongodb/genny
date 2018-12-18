#include <runActorHelper.hpp>
#include <thread>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <vector>

namespace genny {

void ActorHelper::run(const ActorHelper::FuncWithContext&& runnerFunc) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    orchestrator.addRequiredTokens(_tokenCount);

    metrics::Registry registry;
    WorkloadContext wl(
        _config, registry, orchestrator, "mongodb://localhost:27017", *_cast);

    runnerFunc(wl);
}

void ActorHelper::_doRunThreaded(const WorkloadContext& wl) {
    std::vector<std::thread> threads;
    std::transform(cbegin(wl.actors()),
                   cend(wl.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) { return std::thread{[&]() { actor->run(); }}; });

    for (auto& thread : threads)
        thread.join();
}
}
