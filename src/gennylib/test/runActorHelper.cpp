#include <runActorHelper.hpp>
#include <thread>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <vector>

namespace genny {

ActorHelper::ActorHelper(
    const YAML::Node& config,
    int tokenCount,
    const std::initializer_list<Cast::ActorProducerMap::value_type>&& castInitializer) {
    genny::metrics::Registry metrics;
    genny::Orchestrator orchestrator{metrics.gauge("PhaseNumber")};
    orchestrator.addRequiredTokens(tokenCount);

    metrics::Registry registry;
    _wlc = std::make_unique<WorkloadContext>(
        config, registry, orchestrator, "mongodb://localhost:27017", Cast(castInitializer));
}

void ActorHelper::run(ActorHelper::FuncWithContext&& runnerFunc) {
    runnerFunc(*_wlc);
}

void ActorHelper::runAndVerify(ActorHelper::FuncWithContext&& runnerFunc,
                               ActorHelper::FuncWithContext&& verifyFunc) {
    runnerFunc(*_wlc);
    verifyFunc(*_wlc);
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
}  // namespace genny
