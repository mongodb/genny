#ifndef HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
#define HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED

#include <iostream>

#include <gennylib/metrics.hpp>

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
    constexpr explicit Reporter(const Registry& registry) : _registry{std::addressof(registry)} {}

    /** @return how many distinct gauges were registered */
    auto getGaugeCount(V1::Permission perm= {}) const {
        auto& x = _registry->getGauges(perm);
        return std::distance(x.begin(), x.end());
    }

    /** @return how many gauge data-points were recorded */
    long getGaugePointsCount(V1::Permission perm= {}) const {
        return dataPointsCount(_registry->getGauges(perm),perm);
    }

    /** @return how many distinct timers were registered */
    auto getTimerCount(V1::Permission perm= {}) const {
        auto& x = _registry->getTimers(perm);
        return std::distance(x.begin(), x.end());
    }

    /** @return how many timer data-points were recorded */
    long getTimerPointsCount(V1::Permission perm= {}) const {
        return dataPointsCount(_registry->getTimers(perm), perm);
    }

    /** @return how many counters were registered */
    auto getCounterCount(V1::Permission perm= {}) const {
        auto& x = _registry->getCounters(perm);
        return std::distance(x.begin(), x.end());
    }

    /** @return how many counter data-points were recorded */
    long getCounterPointsCount(V1::Permission perm= {}) const {
        return dataPointsCount(_registry->getCounters(perm), perm);
    }

    /**
     * @param out print a human-readable listing of all
     *            data-points to this ostream.
     */
    void report(std::ostream& out, V1::Permission perm= {}) const {
        out << "counters" << std::endl;
        doReport(out, _registry->getCounters(perm),perm);
        out << std::endl;

        out << "gauges" << std::endl;
        doReport(out, _registry->getGauges(perm),perm);
        out << std::endl;

        out << "timers" << std::endl;
        doReport(out, _registry->getTimers(perm), perm);
        out << std::endl;
    }

private:
/**
 * Prints a map<string,X> where X is a CounterImpl, GaugeImpl, etc.
 * @tparam X is a map<string,V> where V has getTimeSeries() that exposes a metrics::TimeSeries
 */
    template <typename X> static
    void doReport(std::ostream& out, const X& haveTimeSeries, genny::metrics::V1::Permission perm) {
        for (const auto& c: haveTimeSeries) {
            for (const auto& v : c.second.getTimeSeries(perm).getVals(perm)) {
                out << v.first.time_since_epoch().count()
                    << "," << c.first << "," << v.second
                    << std::endl;
            }
        }
    }


/**
 * @return the number of data-points held by a map with values CounterImpl, GaugeImpl, etc.
 */
    template <class X> static
    long dataPointsCount(const X& x, genny::metrics::V1::Permission perm) {
        auto out = 0L;
        for(const auto& v : x) {
            out += v.second.getTimeSeries(perm).getDataPointCount(perm);
        }
        return out;
    }
    const Registry* _registry;
};

}  // namespace genny::metrics

#endif  // HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
