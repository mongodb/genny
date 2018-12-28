#ifndef HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
#define HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED

#include <functional>

#include <yaml-cpp/yaml.h>

#include <gennylib/Cast.hpp>

namespace genny {

class Orchestrator;
class WorkloadContext;

namespace metrics {
class Registry;
}

/**
 * Helper class to run an actor for a test. No metrics are collected by default.
 *
 * @param config YAML config of a workload that includes the actors you want to run.
 * @param tokenCount The total number of simultaneous threads ("tokens" in
 * Orchestrator lingo) required by all actors.
 * @param l initializer list for a Cast.
 */
class ActorHelper {
public:
    using FuncWithContext = std::function<void(const WorkloadContext&)>;

    ActorHelper(const YAML::Node& config, int tokenCount, Cast::List castInitializer);

    void run(FuncWithContext&& runnerFunc = ActorHelper::doRunThreaded);

    void runAndVerify(FuncWithContext&& runnerFunc = ActorHelper::doRunThreaded,
                      FuncWithContext&& verifyFunc = FuncWithContext());

    void runDefaultAndVerify(FuncWithContext&& verifyFunc) {
        runAndVerify(ActorHelper::doRunThreaded, std::forward<FuncWithContext>(verifyFunc));
    }

    static void doRunThreaded(const WorkloadContext& wl);

private:
    // These are only used when constructing the workload context, but the context doesn't own them.
    std::unique_ptr<metrics::Registry> _registry;
    std::unique_ptr<Orchestrator> _orchestrator;
    std::unique_ptr<Cast> _cast;

    std::unique_ptr<WorkloadContext> _wlc;
};
}  // namespace genny

#endif  // HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
