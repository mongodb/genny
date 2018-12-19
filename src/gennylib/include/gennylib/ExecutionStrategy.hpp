#ifndef HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861
#define HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861

#include <mongocxx/exception/operation_exception.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/config/ExecutionStrategy.hpp>
#include <gennylib/metrics.hpp>

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

    using RunOptions = config::ExecutionStrategy;

public:
    ExecutionStrategy(ActorContext& context, ActorId id, const std::string& operation);
    ~ExecutionStrategy();

    template <typename F>
    void run(F&& fun, const RunOptions& options = RunOptions{}) {
        Result result;

        for (; result.numAttempts <= options.maxRetries; ++result.numAttempts) {
            try {
                auto timer = _timer.start();
                ++_ops;

                fun();

                timer.report();
                result.wasSuccessful = true;
            } catch (const mongocxx::operation_exception& e) {
                _recordError(e);
            }
        }

        _finishRun(options, std::move(result));
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
    void _recordError(const mongocxx::operation_exception& e);
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
