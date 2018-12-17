#include <gennylib/MongoWrapper.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/context.hpp>

namespace genny {
MongoWrapper::MongoWrapper(ActorContext& context, const std::string& metricsPrefix)
    : _errorGauge{context.gauge(metricsPrefix + ".errors")},
      _opsGauge{context.gauge(metricsPrefix + ".ops")},
      _timer{context.timer(metricsPrefix + ".op-time")} {}

MongoWrapper::~MongoWrapper() {
    markOps();
}

void MongoWrapper::markOps() {
    _opsGauge.set(_ops);
}

void MongoWrapper::_recordError(const mongocxx::operation_exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error #" << _errors << ": " << e.what();

    _errorGauge.set(++_errors);
}
} // namespace genny
