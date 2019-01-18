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

#ifndef HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861
#define HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861

#include <mongocxx/exception/operation_exception.hpp>

#include <loki/ScopeGuard.h>

#include <gennylib/Actor.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/config/ExecutionStrategyOptions.hpp>
#include <metrics/metrics.hpp>

namespace genny {

class ActorContext;

/**
 * A small wrapper for running Mongo commands and recording metrics.
 *
 * This class is intended to make it painless and safe to run mongo commands that may throw
 * operation_exceptions. It maintains a timer for successful operations, an always-updated genny
 * gauge for failed operations, and an amortized guage for all operation. The ExecutionStrategy also
 * allows the user to specify a maximum number of retries for failed operations. Note that failed
 * operations do not throw -- It is the user's responsibility to check `lastResult()` when different
 * behavior is desired for failed operations.
 */
class ExecutionStrategy {
public:
    struct Result {
        bool wasSuccessful = false;
        size_t numAttempts = 0;
    };

    using RunOptions = config::ExecutionStrategyOptions;

public:
    ExecutionStrategy(ActorContext& context, ActorId id, const std::string& operation);
    ~ExecutionStrategy();

    /*
     * Either get a set of options at the specified path in the config,
     * or return a default constructed set of the options.
     * This function is mostly about abstracting a fairly common pattern for DRYness
     */
    template <typename ConfigT, class... Args>
    static RunOptions getOptionsFrom(const ConfigT& config, Args&&... args) {
        return config.template get<RunOptions, false>(std::forward<Args>(args)...)
            .value_or(RunOptions{});
    }

    template <typename F>
    void run(F&& fun, const RunOptions& options = RunOptions{}) {
        Result result;

        // Always report our results, even if we threw
        auto guard = Loki::MakeGuard([&]() { _finishRun(options, std::move(result)); });

        bool shouldContinue = true;
        while (shouldContinue) {
            try {
                auto timer = _timer.start();
                ++_ops;
                ++result.numAttempts;

                fun();

                timer.report();

                result.wasSuccessful = true;
                shouldContinue = false;
            } catch (const boost::exception& e) {
                _recordError(e);

                // We should continue if we've attempted less than the amount of retries plus one
                // for the original attempt
                shouldContinue = result.numAttempts <= options.maxRetries;
                if (!shouldContinue) {
                    result.wasSuccessful = false;

                    if (options.throwOnFailure) {
                        throw;
                    }
                }
            }
        }
    }

    // This function takes the existing counter metrics recorded in this class, and adds them into
    // the metrics namespace timeseries. This may be desired in specific edge cases or at the end of
    // each phase. This will always be called when the ExecutionStrategy is dtor'd.
    void recordMetrics();

    size_t errors() const {
        return _errors;
    }
    const Result& lastResult() const {
        return _lastResult;
    }

private:
    void _recordError(const boost::exception& e);
    void _finishRun(const RunOptions& options, Result result);

    Result _lastResult;

    // The total number of times that the execution function has not succeeded.
    size_t _errors = 0;
    metrics::Gauge _errorGauge;

    // The total number of times that the execution function has been invoked.
    size_t _ops = 0;
    metrics::Gauge _opsGauge;

    // The time series of execution function starts and _successful_ stops.
    metrics::Timer _timer;
};
}  // namespace genny

#endif  // HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861
