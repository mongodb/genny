#include <gennylib/ExecutionStrategy.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/context.hpp>

namespace genny {
ExecutionStrategy::MongoWrapper(ActorContext& context, const std::string& metricsPrefix)
    : _errorGauge{context.gauge(metricsPrefix + ".errors")},
      _opsGauge{context.gauge(metricsPrefix + ".ops")},
      _timer{context.timer(metricsPrefix + ".op-time")} {}

ExecutionStrategy::~MongoWrapper() {
    markOps();
}

void ExecutionStrategy::markOps() {
    _opsGauge.set(_ops);
}

void ExecutionStrategy::_recordError(const mongocxx::operation_exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error #" << _errors << ": " << e.what();

    _errorGauge.set(++_errors);
}
} // namespace genny
