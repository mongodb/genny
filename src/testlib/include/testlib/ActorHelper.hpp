// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
#define HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED

#include <functional>
#include <string>

#include <gennylib/Cast.hpp>
#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny {

/**
 * Helper class to run an Actor for a test. No metrics are collected by default.
 */
class ActorHelper {
public:
    using FuncWithContext = std::function<void(const WorkloadContext&)>;

    /**
     * Construct an ActorHelper with a cast.
     *
     * @param config YAML config of a workload that includes the actors you want to run.
     * @param tokenCount The total number of simultaneous threads ("tokens" in
     * Orchestrator lingo) required by all actors.
     * @param l initializer list for a Cast.
     * @param clientOpts optional, options to the mongocxx::client.
     */
    ActorHelper(const Node& config,
                int tokenCount,
                Cast::List castInitializer,
                v1::PoolManager::OnCommandStartCallback apmCallback = {});

    /**
     * Construct an ActorHelper with the global cast.
     */
    ActorHelper(const Node& config,
                int tokenCount,
                v1::PoolManager::OnCommandStartCallback apmCallback = {});

    void run(FuncWithContext&& runnerFunc);

    void run();

    void runAndVerify(FuncWithContext&& runnerFunc, FuncWithContext&& verifyFunc);

    void runDefaultAndVerify(FuncWithContext&& verifyFunc) {
        doRunThreaded(*_wlc);
        verifyFunc(*_wlc);
    }

    void doRunThreaded(const WorkloadContext& wl);

    const std::string getMetricsOutput() {
        return _metricsOutput.str();
    }

    auto client() {
        return workload()->getClient("Default");
    }

    WorkloadContext* workload() {
        return _wlc.get();
    }

private:
    // These are only used when constructing the workload context, but the context doesn't own
    // them.
    std::unique_ptr<Orchestrator> _orchestrator;
    std::unique_ptr<Cast> _cast;

    std::unique_ptr<WorkloadContext> _wlc;

    std::stringstream _metricsOutput;
};
}  // namespace genny

#endif  // HEADER_C4365A3F_5581_470B_8B54_B46A42795A62_INCLUDED
