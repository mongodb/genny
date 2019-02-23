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
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v1 namespace should ever be typed directly into
 * the implementation of an actor.
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
                   long long systemTime,
                   long long metricsTime,
                   v1::Permission perm) const {
        out << "Clocks" << std::endl;
        writeClocks(out, systemTime, metricsTime);
        out << std::endl;

        out << "Counters" << std::endl;
        writeMetricValues(out, "_bytes", perm, [](const OperationEvent<MetricsClockSource>& event) {
            return event.size;
        });
        writeMetricValues(out, "_docs", perm, [](const OperationEvent<MetricsClockSource>& event) {
            return event.ops;
        });
        writeMetricValues(out, "_iters", perm, [](const OperationEvent<MetricsClockSource>& event) {
            return event.iters;
        });
        out << std::endl;

        out << "Gauges" << std::endl;
        out << std::endl;

        out << "Timers" << std::endl;
        writeMetricValues(out, "_timer", perm, [](const OperationEvent<MetricsClockSource>& event) {
            return nanosecondsCount(
                static_cast<typename MetricsClockSource::duration>(event.duration));
        });
        out << std::endl;
    }

    void writeClocks(std::ostream& out, long long systemTime, long long metricsTime) const {
        out << "SystemTime"
            << "," << systemTime << std::endl;
        out << "MetricsTime"
            << "," << metricsTime << std::endl;
    }

    // TODO: Implement special rules for Genny.Setup and Genny.ActiveActors in order to keep the
    // "csv" metrics format the same as before.
    //
    // 1. We need to not include ".id-0" in the metric name.
    // 2. We need to ignore "Genny.Setup" in the Counters section.
    // 3. We need to ignore "Genny.ActorStarted" in the Timers section.
    // 4. We need to ignore "Genny.ActorFinished" in the Timers section.
    // 5. We need to report a "Genny.ActiveActors" metric by walking the "Genny.ActorStarted" and
    //    "Genny.ActorFinished" time series and translating the increments and decrements into a
    //    total count.
    static std::ostream& writeMetricName(std::ostream& out,
                                         ActorId actorId,
                                         const std::string& actorName,
                                         const std::string& opName) {
        out << actorName << ".id-" << std::to_string(actorId) << "." << opName;
        return out;
    }

    /**
     * @return the number of nanoseconds represented by the duration.
     * @param dur the duration
     * @tparam DurationIn the duration's type
     */
    template <typename DurationIn>
    static count_type nanosecondsCount(const DurationIn& dur) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count();
    }

    void writeMetricValues(
        std::ostream& out,
        const std::string& suffix,
        Permission perm,
        std::function<count_type(const OperationEvent<MetricsClockSource>&)> getter) const {
        for (const auto& [actorName, opsByType] : _registry->getOps(perm)) {
            for (const auto& [opName, opsByThread] : opsByType) {
                for (const auto& [actorId, timeSeries] : opsByThread) {
                    for (const auto& event : timeSeries) {
                        out << nanosecondsCount(event.first.time_since_epoch());
                        out << ",";
                        writeMetricName(out, actorId, actorName, opName) << suffix;
                        out << ",";
                        out << getter(event.second);
                        out << std::endl;
                    }
                }
            }
        }
    }

    const RegistryT<MetricsClockSource>* const _registry;
};

}  // namespace v1

using Reporter = v1::ReporterT<Registry::clock>;

}  // namespace genny::metrics

#endif  // HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
