#ifndef HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
#define HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED

#include <yaml-cpp/yaml.h>

#include <gennylib/Cast.hpp>

namespace genny {

class WorkloadContext;

/**
 * Helper class to run an actor for a test. No metrics are collected and no
 *
 * @param config YAML config of a workload that includes the actors you want to run.
 * @param tokenCount The total number of simultaneous threads ("tokens" in
 * Orchestrator lingo) required by all actors.
 * @param l initializer list for a Cast.
 */
class ActorHelper {
public:
    using FuncWithContext = std::function<void(const WorkloadContext&)>;

    ActorHelper(const YAML::Node& config,
                int tokenCount,
                const std::initializer_list<Cast::ActorProducerMap::value_type>&& castInitializer)
        : _config(config),
          _tokenCount(tokenCount),
          _cast(std::make_unique<Cast>(castInitializer)){};

    /**
     * Run the actors.
     */
    void run(FuncWithContext&& runnerFunc = ActorHelper::_doRunThreaded);

    /**
     *  Run the actors and verify the results using verifyFunc.
     */
    void runAndVerify(ActorHelper::FuncWithContext&& runnerFunc = ActorHelper::_doRunThreaded,
                      std::function<void()>&& verifyFunc = []() {});

private:
    const YAML::Node& _config;
    int _tokenCount;
    std::unique_ptr<Cast> _cast;

    static void _doRunThreaded(const WorkloadContext& wl);
};
}  // namespace genny

#endif  // HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
