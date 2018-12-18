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
    // TODO this should take a RegistryHandle for its actor instead of a prefix
    ExecutionStrategy(ActorContext & context, const std::string & metricsPrefix);
    ~ExecutionStrategy();

    template<typename F>
    bool tryToRun(F&& fun){
        try {
            // Does it make sense to have a split timer?
            auto timer = _timer.start(); 
            ++_ops;

            fun();

            timer.report();
            return true;
        } catch (const mongocxx::operation_exception & e){
            _recordError(e);
            return false;
        }
    }
    
    void markOps();

    size_t errors() const { return _errors; }

private:
    void _recordError(const mongocxx::operation_exception & e);

    size_t _errors = 0;
    metrics::Gauge _errorGauge;

    size_t _ops = 0;
    metrics::Gauge _opsGauge;

    metrics::Timer _timer;
};
} // namespace genny

#endif // HEADER_0BE8D22D_E93B_48FE_BC5A_CFFF2E05D861
