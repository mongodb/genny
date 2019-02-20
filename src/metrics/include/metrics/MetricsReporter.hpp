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

#ifndef HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
#define HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED

#include <iostream>

#include <metrics/metrics.hpp>

namespace genny::metrics {

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by Genny's
 * internals. Actors should never have to type `genny::*::v1` into any types.
 */
namespace v1 {

/**
 * A ReporterT is the only object in the system that
 * has read access to metrics data-points. It is not
 * intended to be used by actors, only by drivers.
 *
 * The ReporterT is given read-access to metrics data
 * for the purposes of reporting data.
 * This class is not ABI-safe.
 *
 * @private
 */
template <typename MetricsClockSource>
class ReporterT {

public:
    constexpr explicit ReporterT(const v1::RegistryT<MetricsClockSource>& registry)
        : _registry{std::addressof(registry)} {}

    /** @return how many distinct gauges were registered */
    auto getGaugeCount() const {
        auto& x = _registry->getGauges(v1::Permission{});
        return std::distance(x.begin(), x.end());
    }

    /** @return how many gauge data-points were recorded */
    long getGaugePointsCount() const {
        v1::Permission perm;
        return dataPointsCount(_registry->getGauges(perm), perm);
    }

    /** @return how many distinct timers were registered */
    auto getTimerCount() const {
        auto& x = _registry->getTimers(v1::Permission{});
        return std::distance(x.begin(), x.end());
    }

    /** @return how many timer data-points were recorded */
    long getTimerPointsCount() const {
        v1::Permission perm;
        return dataPointsCount(_registry->getTimers(perm), perm);
    }

    /** @return how many counters were registered */
    auto getCounterCount() const {
        auto& x = _registry->getCounters(v1::Permission{});
        return std::distance(x.begin(), x.end());
    }

    /** @return how many counter data-points were recorded */
    long getCounterPointsCount() const {
        v1::Permission perm;
        return dataPointsCount(_registry->getCounters(perm), perm);
    }

private:
    struct SystemClockSource {
        using clock = std::chrono::system_clock;

        static std::chrono::time_point<clock> now() {
            return clock::now();
        }
    };

public:
    /**
     * @param out print a human-readable listing of all
     *            data-points to this ostream.
     * @param metricsFormat the format to use. Must be "csv".
     */
    template <typename ReporterClockSource = SystemClockSource>
    void report(std::ostream& out, const std::string& metricsFormat) const {
        v1::Permission perm;

        // should these values come from the registry, and should they be recorded at
        // time of registry-creation?
        auto systemTime = nanosecondsCount(ReporterClockSource::now().time_since_epoch());
        auto metricsTime = nanosecondsCount(_registry->now(perm).time_since_epoch());

        // if this lives more than a hot-second, put the formats into an enum and do this
        // check & throw in the driver/main program
        if (metricsFormat == "csv") {
            return reportCsv(out, systemTime, metricsTime, perm);
        } else {
            throw std::invalid_argument(std::string("Unknown metrics format ") + metricsFormat);
        }
    }

private:
    void reportCsv(std::ostream& out,
                   long long int systemTime,
                   long long int metricsTime,
                   v1::Permission perm) const {
        out << "Clocks" << std::endl;
        doClocks(out, systemTime, metricsTime);
        out << std::endl;

        out << "Counters" << std::endl;
        doReport(out, _registry->getCounters(perm), perm);
        out << std::endl;

        out << "Gauges" << std::endl;
        doReport(out, _registry->getGauges(perm), perm);
        out << std::endl;

        out << "Timers" << std::endl;
        doReport(out, _registry->getTimers(perm), perm);
        out << std::endl;
    }

    void doClocks(std::ostream& out, long long int systemTime, long long int metricsTime) const {
        out << "SystemTime"
            << "," << systemTime << std::endl;
        out << "MetricsTime"
            << "," << metricsTime << std::endl;
    }

    /**
     * Prints a map<string,X> where X is a CounterImpl, GaugeImpl, etc.
     * @tparam X is a map<string,V> where V has getTimeSeries() that exposes a metrics::TimeSeries
     */
    template <typename X>
    static void doReport(std::ostream& out,
                         const X& haveTimeSeries,
                         genny::metrics::v1::Permission perm) {
        for (const auto& c : haveTimeSeries) {
            for (const auto& v : c.second.getTimeSeries(perm).getVals(perm)) {
                out << nanosecondsCount(v.first.time_since_epoch()) << "," << c.first << ","
                    << v.second << std::endl;
            }
        }
    }


    /**
     * @return the number of data-points held by a map with values CounterImpl, GaugeImpl, etc.
     */
    template <class X>
    static long dataPointsCount(const X& x, genny::metrics::v1::Permission perm) {
        auto out = 0L;
        for (const auto& v : x) {
            out += v.second.getTimeSeries(perm).getDataPointCount(perm);
        }
        return out;
    }

    /**
     * @return the number of nanoseconds represented by the duration.
     * @param dur the duration
     * @tparam DurationIn the duration's type
     */
    template <typename DurationIn>
    static long long nanosecondsCount(const DurationIn& dur) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
    }

    const v1::RegistryT<MetricsClockSource>* _registry;
};

}  // namespace v1

using Reporter = v1::ReporterT<Registry::clock>;

}  // namespace genny::metrics

#endif  // HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
