#ifndef HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861
#define HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861

#include <optional>

#include <mongocxx/exception/operation_exception.hpp>

#include <gennylib/metrics.hpp>

namespace genny {

class ActorContext;

/**
 * A small package of tools for running Mongo commands and recording metrics
 *
 * ...it's got sick dBeats.
 */
class ExecutionStrategy {
public:
    struct Result{
        bool wasSuccessful = false;
        size_t numAttempts = 0;
    };

    struct RunOptions{
        size_t maxRetries = 0;
    };
public:
    // TODO this should take a RegistryHandle for its actor instead of a prefix
    ExecutionStrategy(ActorContext & context, const std::string & metricsPrefix);
    ~ExecutionStrategy();

    template<typename F>
    void tryToRun(F&& fun, const RunOptions & options = RunOptions{}){
        Result result;

        for (; result.numAttempts <= options.maxRetries; ++result.numAttempts) {
            try {
                // Does it make sense to have a split timer?
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
    
    void recordMetrics();

    size_t errors() const { return _errors; }
    const Result & lastResult() const { return _lastResult; }
    

private:
    void _recordError(const mongocxx::operation_exception & e);
    void _finishRun(const RunOptions & options, Result result);

    Result _lastResult;

    size_t _errors = 0;
    metrics::Gauge _errorGauge;

    size_t _ops = 0;
    metrics::Gauge _opsGauge;

    metrics::Timer _timer;
};
} // namespace genny

#endif // HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861
