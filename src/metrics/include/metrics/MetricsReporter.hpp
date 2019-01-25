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
 * A Reporter is the only object in the system that
 * has read access to metrics data-points. It is not
 * intended to be used by actors, only by drivers.
 *
 * The Reporter is given read-access to metrics data
 * for the purposes of reporting data.
 * This class is not ABI-safe.
 *
 * @private
 */
class Reporter {

public:
    constexpr explicit Reporter(const Registry& registry) : _registry{std::addressof(registry)} {}

    /** @return how many distinct gauges were registered */
    auto getGaugeCount(V1::Permission perm = {}) const {
        auto& x = _registry->getGauges(perm);
        return std::distance(x.begin(), x.end());
    }

    /** @return how many gauge data-points were recorded */
    long getGaugePointsCount(V1::Permission perm = {}) const {
        return dataPointsCount(_registry->getGauges(perm), perm);
    }

    /** @return how many distinct timers were registered */
    auto getTimerCount(V1::Permission perm = {}) const {
        auto& x = _registry->getTimers(perm);
        return std::distance(x.begin(), x.end());
    }

    /** @return how many timer data-points were recorded */
    long getTimerPointsCount(V1::Permission perm = {}) const {
        return dataPointsCount(_registry->getTimers(perm), perm);
    }

    /** @return how many counters were registered */
    auto getCounterCount(V1::Permission perm = {}) const {
        auto& x = _registry->getCounters(perm);
        return std::distance(x.begin(), x.end());
    }

    /** @return how many counter data-points were recorded */
    long getCounterPointsCount(V1::Permission perm = {}) const {
        return dataPointsCount(_registry->getCounters(perm), perm);
    }

    /**
     * @param out print a human-readable listing of all
     *            data-points to this ostream.
     * @param metricsFormat the format to use. Either "csv" or "sys-perf".
     * @param perm passkey permission (must be friends with metrics::Registry)
     */
    void report(std::ostream& out,
                const std::string& metricsFormat,
                V1::Permission perm = {}) const {
        // should these values come from the registry, and should they be recorded at
        // time of registry-creation?
        auto systemTime = std::chrono::system_clock::now().time_since_epoch().count();
        auto metricsTime = _registry->now(perm).time_since_epoch().count();

        // if this lives more than a hot-second, put the formats into an enum and do this
        // check & throw in the driver/main program
        if (metricsFormat == "csv") {
            return reportCsv(out, systemTime, metricsTime, perm);
        } else if (metricsFormat == "sys-perf") {
            return reportSysperf(out, systemTime, metricsTime, perm);
        } else {
            throw std::invalid_argument(std::string("Unknown metrics format ") + metricsFormat);
        }
    }

private:
    template <class T>
    class TD;

    void reportSysperf(std::ostream& out,
                       long long int,
                       long long int,
                       const V1::Permission& perm) const {

        // TODO: Followup from TIG-1070, make this report real data; for now just report
        //       the number of timer data-points that we saw.

        genny::metrics::clock::duration total;
        for (const auto& [name, timer] : _registry->getTimers(perm)) {
            auto& vals = timer.getTimeSeries(perm).getVals(perm);
            for (const auto& [when, dur] : vals) {
                total += dur;
            }
        }

        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(total);

        out << R"({"storageEngine":"wiredTiger", "results":[{
 "name":"dummy_inserts", "workload":"genny_dummy_workload",
 "start":1537815283.968272, "end":1537817860.423682,
 "results":{"4":{"ops_per_sec":)"
            << micros.count() << R"(, "ops_per_sec_values":[)" << micros.count() << R"(]}}}]}
)";
    }
    void reportCsv(std::ostream& out,
                   long long int systemTime,
                   long long int metricsTime,
                   const V1::Permission& perm) const {
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
                         genny::metrics::V1::Permission perm) {
        for (const auto& c : haveTimeSeries) {
            for (const auto& v : c.second.getTimeSeries(perm).getVals(perm)) {
                out << v.first.time_since_epoch().count() << "," << c.first << "," << v.second
                    << std::endl;
            }
        }
    }


    /**
     * @return the number of data-points held by a map with values CounterImpl, GaugeImpl, etc.
     */
    template <class X>
    static long dataPointsCount(const X& x, genny::metrics::V1::Permission perm) {
        auto out = 0L;
        for (const auto& v : x) {
            out += v.second.getTimeSeries(perm).getDataPointCount(perm);
        }
        return out;
    }
    const Registry* _registry;
};

}  // namespace genny::metrics

#endif  // HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
