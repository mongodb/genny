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

#include <boost/exception/diagnostic_information.hpp>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/context.hpp>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>

namespace genny {

ActorHelper::ActorHelper(const Node& config,
                         int tokenCount,
                         Cast::List castInitializer,
                         const std::string& uri,
                         v1::PoolManager::OnCommandStartCallback apmCallback) {
    if (tokenCount <= 0) {
        throw InvalidConfigurationException("Must add a positive number of tokens");
    }

    _orchestrator = std::make_unique<genny::Orchestrator>();
    _orchestrator->addRequiredTokens(tokenCount);

    _cast = std::make_unique<Cast>(castInitializer);
    try {
        _wlc = std::make_unique<WorkloadContext>(config, *_orchestrator, uri, *_cast, apmCallback);
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(x, true);
        throw;
    }
}

ActorHelper::ActorHelper(const Node& config,
                         int tokenCount,
                         const std::string& uri,
                         v1::PoolManager::OnCommandStartCallback apmCallback) {
    if (tokenCount <= 0) {
        throw InvalidConfigurationException("Must add a positive number of tokens");
    }

    _orchestrator = std::make_unique<genny::Orchestrator>();
    _orchestrator->addRequiredTokens(tokenCount);

    try {
        _wlc = std::make_unique<WorkloadContext>(
            config, *_orchestrator, uri, globalCast(), apmCallback);
    } catch (const std::exception& x) {
        BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(x, true);
        throw;
    }
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
    std::lock_guard<const ActorVector> actorsLock(wl.actors());
    std::transform(cbegin(wl.actors()),
                   cend(wl.actors()),
                   std::back_inserter(threads),
                   [&](const auto& actor) {
                       return std::thread{[&]() {
                           try {
                               actor->run();
                           } catch (const boost::exception& b) {
                               BOOST_LOG_TRIVIAL(error) << boost::diagnostic_information(b, true);
                               throw;
                           }
                       }};
                   });

    for (auto& thread : threads)
        thread.join();

    auto reporter = genny::metrics::Reporter{_wlc->getMetrics()};

    reporter.report(_metricsOutput, metrics::MetricsFormat("csv"));
}
}  // namespace genny
