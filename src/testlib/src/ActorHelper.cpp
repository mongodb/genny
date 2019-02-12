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

#include <testlib/ActorHelper.hpp>

#include <thread>
#include <vector>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>

namespace genny {

ActorHelper::ActorHelper(const YAML::Node& config,
                         int tokenCount,
                         Cast::List castInitializer,
                         const std::string& uri,
                         ApmCallback apmCallback) {
    if (tokenCount <= 0) {
        throw InvalidConfigurationException("Must add a positive number of tokens");
    }

    _registry = std::make_unique<genny::metrics::Registry>();

    _orchestrator = std::make_unique<genny::Orchestrator>();
    _orchestrator->addRequiredTokens(tokenCount);

    _cast = std::make_unique<Cast>(castInitializer);
    _wlc = std::make_unique<WorkloadContext>(
        config, *_registry, *_orchestrator, uri, *_cast, apmCallback);
}

ActorHelper::ActorHelper(const YAML::Node& config,
                         int tokenCount,
                         const std::string& uri,
                         ApmCallback apmCallback) {
    if (tokenCount <= 0) {
        throw InvalidConfigurationException("Must add a positive number of tokens");
    }

    _registry = std::make_unique<genny::metrics::Registry>();

    _orchestrator = std::make_unique<genny::Orchestrator>();
    _orchestrator->addRequiredTokens(tokenCount);

    _wlc = std::make_unique<WorkloadContext>(
        config, *_registry, *_orchestrator, uri, globalCast(), apmCallback);
}

void ActorHelper::run() {
    doRunThreaded(*_wlc);
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

    auto reporter = genny::metrics::Reporter{*_registry};

    reporter.report(_metricsOutput, "csv");
}
}  // namespace genny
