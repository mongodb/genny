#include <ActorHelper.hpp>

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

    if (tokenCount <= 0) {
        throw InvalidConfigurationException("Must add a positive number of tokens");
    }

    _registry = std::make_unique<genny::metrics::Registry>();

    _orchestrator = std::make_unique<genny::Orchestrator>(_registry->gauge("PhaseNumber"));
    _orchestrator->addRequiredTokens(tokenCount);

    _cast = std::make_unique<Cast>(castInitializer);

    _wlc = std::make_unique<WorkloadContext>(config, *_registry, *_orchestrator, "mongodb://localhost:27017", *_cast);
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
