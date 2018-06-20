#ifndef HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
#define HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED

#include <iostream>

#include <gennylib/metrics.hpp>

namespace {

std::ostream& operator<<(std::ostream& out, const genny::metrics::period& p) {
    out << p.count();
    return out;
}

/**
 * Prints a map<string,X> where X is a CounterImpl, GaugeImpl, etc.
 * @tparam X is a map<string,V> where V has getTimeSeries() that exposes a metrics::TimeSeries
 */
template <typename X>
void doReport(std::ostream& out, X& haveTimeSeries) {
    auto perm = genny::metrics::V1::Permission{};
    for (auto&& c: haveTimeSeries) {
        for (auto&& v : c.second.getTimeSeries(perm).getVals(perm)) {
            out << v.first.time_since_epoch().count()
                << "," << c.first << "," << v.second
                << std::endl;
        }
    }
}


/**
 * @return the number of data-points held by a map with values CounterImpl, GaugeImpl, etc.
 */
template <class X>
long dataPointsCount(X& x) {
    auto perm = genny::metrics::V1::Permission{};

    auto out = 0L;
    for(auto& v : x) {
        out += v.second.getTimeSeries(perm).getDataPointCount(perm);
    }
    return out;
}


}  // namespace


namespace genny::metrics {

/**
 * A Reporter is the only object in the system that
 * has read access to metrics data-points. It is not
 * intended to be used by actors, only by drivers.
 *
 * The Reporter is given read-access to metrics data
 * for the purposes of reporting data.
 * This class is not ABI-safe.
 */
class Reporter {

public:
    constexpr explicit Reporter(Registry& registry) : _registry{std::addressof(registry)} {}

    /** @return how many distinct gauges were registered */
    auto getGaugeCount() {
        auto& x = _registry->getGauges(V1::Permission{});
        return std::distance(x.begin(), x.end());
    }

    /** @return how many gauge data-points were recorded */
    long getGaugePointsCount() {
        return dataPointsCount(_registry->getGauges(V1::Permission{}));
    }

    /** @return how many distinct timers were registered */
    auto getTimerCount() {
        auto& x = _registry->getTimers(V1::Permission{});
        return std::distance(x.begin(), x.end());
    }

    /** @return how many timer data-points were recorded */
    long getTimerPointsCount() {
        return dataPointsCount(_registry->getTimers(V1::Permission{}));
    }

    /** @return how many counters were registered */
    auto getCounterCount() {
        auto& x = _registry->getCounters(V1::Permission{});
        return std::distance(x.begin(), x.end());
    }

    /** @return how many counter data-points were recorded */
    long getCounterPointsCount() {
        return dataPointsCount(_registry->getCounters(V1::Permission{}));
    }

    /**
     * @param out print a human-readable listing of all
     *            data-points to this ostream.
     */
    void report(std::ostream& out) {
        out << "counters" << std::endl;
        doReport(out, _registry->getCounters(V1::Permission{}));
        out << std::endl;

        out << "gauges" << std::endl;
        doReport(out, _registry->getGauges(V1::Permission{}));
        out << std::endl;

        out << "timers" << std::endl;
        doReport(out, _registry->getTimers(V1::Permission{}));
        out << std::endl;
    }

private:
    Registry* const _registry;
};

}  // namespace genny::metrics

#endif  // HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
