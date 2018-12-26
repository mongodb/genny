#include "ActorHelper.hpp"

#include <thread>
#include <vector>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>


namespace genny {

ActorHelper::ActorHelper(const YAML::Node& config,
                         int tokenCount,
                         Cast::List castInitializer,
                         const std::string& uri) {
    if (tokenCount <= 0) {
        throw InvalidConfigurationException("Must add a positive number of tokens");
    }

    _registry = std::make_unique<genny::metrics::Registry>();

    _orchestrator = std::make_unique<genny::Orchestrator>(_registry->gauge("PhaseNumber"));
    _orchestrator->addRequiredTokens(tokenCount);

    _cast = std::make_unique<Cast>(std::move(castInitializer));

    _wlc = std::make_unique<WorkloadContext>(config, *_registry, *_orchestrator, uri, *_cast);
}

void ActorHelper::run(ActorHelper::FuncWithContext&& runnerFunc) {
    runnerFunc(*_wlc);
}

void ActorHelper::runAndVerify(ActorHelper::FuncWithContext&& runnerFunc,
                               ActorHelper::FuncWithContext&& verifyFunc) {
    runnerFunc(*_wlc);
    verifyFunc(*_wlc);
}

void ActorHelper::doRunThreaded(const WorkloadContext& wl) {
    std::vector<std::thread> threads;
    std::transform(cbegin(wl.actors()),
                   cend(wl.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) { return std::thread{[&]() { actor->run(); }}; });

    for (auto& thread : threads)
        thread.join();
}
}  // namespace genny
