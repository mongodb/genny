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
#include <map>

#include <boost/log/trivial.hpp>

#include <metrics/metrics.hpp>

namespace genny::metrics {

/**
 * @namespace genny::metrics::internals::v1 this namespace is private and only intended to be used
 * by genny's own internals. No types from the genny::metrics::internals::v1 namespace should ever
 * be typed directly into the implementation of an actor.
 *
 * After transitioning to only using ftdc-based metrics, the namespace hierarchy can be simplified.
 */
namespace internals::v1 {

// Used in the implementation of outputting metrics. Only at the top of the file
// because C++ is a delight.
//
// This has to be inline because it'll be included in multiple translation-units.
// We could just have the decl here and make a separate impl .cpp file, but the
// rest of the "metrics" module is header-only and it seems silly to kill that
// just for a single function. If additional 'inline' functions abound in the
// future, please move to a .cpp file to decrease compile- and link-times.
inline void logMaybe(unsigned long long iteration,
                     const std::string& actorName,
                     const std::string& opName) {
    // Log progress every 100e6 iterations
    if (iteration % (100 * 1000 * 1000) == 0) {
        BOOST_LOG_TRIVIAL(info) << "Processed " << iteration << " metrics. Processing " << actorName
                                << "." << opName;
    }
}

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
class ReporterT final {

public:
    constexpr explicit ReporterT(const internals::RegistryT<MetricsClockSource>& registry)
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
    void report(std::ostream& out, const MetricsFormat& metricsFormat) const {
        v1::Permission perm;

        BOOST_LOG_TRIVIAL(debug) << "Beginning metrics reporting.";

        // should these values come from the registry, and should they be recorded at
        // time of registry-creation?
        auto systemTime = nanosecondsCount(ReporterClockSource::now().time_since_epoch());
        auto metricsTime = nanosecondsCount(_registry->now(perm).time_since_epoch());

        // if this lives more than a hot-second, put the formats into an enum and do this
        // check & throw in the driver/main program
        if (metricsFormat.get() == MetricsFormat::Format::kCsv) {
            reportLegacyCsv(out, systemTime, metricsTime, perm);
        } else if (metricsFormat.get() == MetricsFormat::Format::kCedarCsv ||
                   metricsFormat.get() == MetricsFormat::Format::kCsvFtdc) {
            reportCedarCsv(out, systemTime, metricsTime, perm);
        } else {
            throw std::invalid_argument(std::string("Received unknown csv metrics format."));
        }

        BOOST_LOG_TRIVIAL(debug) << "Finished metrics reporting.";
    }

private:
    using duration = typename MetricsClockSource::duration;

    void reportLegacyCsv(std::ostream& out,
                         long long systemTime,
                         long long metricsTime,
                         v1::Permission perm) const {
        out << "Clocks" << std::endl;
        writeClocks(out, systemTime, metricsTime);
        out << std::endl;

        out << "Counters" << std::endl;
        writeGennyActiveActorsMetric(out, perm);
        writeMetricValuesLegacy(
            out, "_bytes", perm, [](const OperationEventT<MetricsClockSource>& event) {
                return event.size;
            });
        writeMetricValuesLegacy(
            out, "_docs", perm, [](const OperationEventT<MetricsClockSource>& event) {
                return event.number;
            });
        writeMetricValuesLegacy(
            out, "_iters", perm, [](const OperationEventT<MetricsClockSource>& event) {
                return event.ops;
            });
        out << std::endl;

        out << "Gauges" << std::endl;
        out << std::endl;

        out << "Timers" << std::endl;
        writeGennySetupMetric(out, perm);
        writeMetricValuesLegacy(
            out, "_timer", perm, [](const OperationEventT<MetricsClockSource>& event) {
                return nanosecondsCount(static_cast<duration>(event.duration));
            });
        out << std::endl;
    }

    void writeClocks(std::ostream& out, long long systemTime, long long metricsTime) const {
        out << "SystemTime"
            << "," << systemTime << std::endl;
        out << "MetricsTime"
            << "," << metricsTime << std::endl;
    }

    static std::ostream& writeMetricNameLegacy(std::ostream& out,
                                               ActorId actorId,
                                               const std::string& actorName,
                                               const std::string& opName) {
        out << actorName << ".id-" << std::to_string(actorId) << "." << opName;
        return out;
    }

    void writeMetricValuesLegacy(
        std::ostream& out,
        const std::string& suffix,
        Permission perm,
        std::function<count_type(const OperationEventT<MetricsClockSource>&)> getter) const {

        size_t iter = 0;

        for (const auto& [actorName, opsByType] : _registry->getOps(perm)) {
            if (actorName == "Genny") {
                // Metrics created by the DefaultDriver are handled separately in order to preserve
                // the legacy "csv" format.
                continue;
            }

            for (const auto& [opName, opsByThread] : opsByType) {
                for (const auto& [actorId, op] : opsByThread) {
                    for (const auto& event : op.getEvents()) {
                        out << nanosecondsCount(event.first.time_since_epoch());
                        out << ",";
                        writeMetricNameLegacy(out, actorId, actorName, opName) << suffix;
                        out << ",";
                        out << getter(event.second);
                        out << std::endl;

                        logMaybe(++iter, actorName, opName);
                    }
                }
            }
        }
    }

    void writeGennySetupMetric(std::ostream& out, Permission perm) const {
        const auto& ops = _registry->getOps(perm);

        auto gennyOpsIt = ops.find("Genny");
        if (gennyOpsIt == ops.end()) {
            // We permit the Genny.Setup metric to be omitted to make unit testing easier.
            return;
        }

        auto setupIt = gennyOpsIt->second.find("Setup");
        if (setupIt == gennyOpsIt->second.end()) {
            // We permit the Genny.Setup metric to be omitted to make unit testing easier.
            return;
        }

        for (const auto& event : setupIt->second.at(0u).getEvents()) {
            out << nanosecondsCount(event.first.time_since_epoch());
            out << ",";
            out << "Genny.Setup";
            out << ",";
            out << nanosecondsCount(static_cast<duration>(event.second.duration));
            out << std::endl;
        }
    }

    void writeGennyActiveActorsMetric(std::ostream& out, Permission perm) const {
        const auto& ops = _registry->getOps(perm);

        auto gennyOpsIt = ops.find("Genny");
        if (gennyOpsIt == ops.end()) {
            // We permit the Genny.ActiveActors metric to be omitted to make unit testing easier.
            return;
        }

        auto startedActorsIt = gennyOpsIt->second.find("ActorStarted");
        if (startedActorsIt == gennyOpsIt->second.end()) {
            // We permit the Genny.ActiveActors metric to be omitted to make unit testing easier.
            return;
        }

        const auto& startedActors = startedActorsIt->second.at(0u);
        const auto& finishedActors = gennyOpsIt->second.find("ActorFinished")->second.at(0u);

        auto startedEventIt = startedActors.getEvents().begin();
        auto finishedEventIt = finishedActors.getEvents().begin();

        auto numActors = 0;
        auto writeMetric = [&](const typename MetricsClockSource::time_point& when) {
            out << nanosecondsCount(when.time_since_epoch());
            out << ",";
            out << "Genny.ActiveActors";
            out << ",";
            out << numActors;
            out << std::endl;
        };

        // The termination condition of the while-loop is based only on `finishedActors` because
        // there should always be as many ActorFinished events as there are ActorStarted events and
        // an actor can only be finished after it has been started.
        while (finishedEventIt != finishedActors.getEvents().end()) {
            if (startedEventIt == startedActors.getEvents().end() ||
                startedEventIt->first > finishedEventIt->first) {
                numActors -= finishedEventIt->second.ops;
                writeMetric(finishedEventIt->first);
                ++finishedEventIt;
            } else if (startedEventIt->first < finishedEventIt->first) {
                numActors += startedEventIt->second.ops;
                writeMetric(startedEventIt->first);
                ++startedEventIt;
            } else {
                // startedEventIt->first == finishedEventIt->first is a violation of the monotonic
                // clock property. This would only happen as a result of a bug in a unit test.
                throw std::logic_error{
                    "Expected time to advance between one actor starting and another actor"
                    " finishing"};
            }
        }
    }

    void reportCedarCsv(std::ostream& out,
                        long long systemTime,
                        long long metricsTime,
                        v1::Permission perm) const {
        out << "Clocks" << std::endl;
        out << "clock,nanoseconds" << std::endl;
        writeClocks(out, systemTime, metricsTime);
        out << std::endl;

        // We use an ordered map here to avoid defining a custom hash function for
        // std::pair<std::string, std::string>. There aren't likely to be many (Actor, Operation)
        // combinations for this to matter too much in terms of efficiency.
        auto opThreadCounts = std::map<std::pair<std::string, std::string>, size_t>{};
        out << "OperationThreadCounts" << std::endl;
        out << "actor,operation,workers" << std::endl;
        for (const auto& [actorName, opsByType] : _registry->getOps(perm)) {
            for (const auto& [opName, opsByThread] : opsByType) {
                if (shouldSkipReporting(actorName, opName)) {
                    continue;
                }

                opThreadCounts[std::make_pair(actorName, opName)] += opsByThread.size();
            }
        }
        for (const auto& [key, count] : opThreadCounts) {
            const auto& [actorName, opName] = key;
            out << actorName << ",";
            out << opName << ",";
            out << count << std::endl;
        }
        out << std::endl;

        unsigned long long iter = 0;

        out << "Operations" << std::endl;
        out << "timestamp,actor,thread,operation,duration,outcome,n,ops,errors,size" << std::endl;
        for (const auto& [actorName, opsByType] : _registry->getOps(perm)) {
            for (const auto& [opName, opsByThread] : opsByType) {
                if (shouldSkipReporting(actorName, opName)) {
                    continue;
                }

                for (const auto& [actorId, op] : opsByThread) {
                    for (const auto& event : op.getEvents()) {
                        out << nanosecondsCount(event.first.time_since_epoch()) << ",";
                        out << actorName << ",";
                        out << actorId << ",";
                        out << opName << ",";
                        out << nanosecondsCount(static_cast<duration>(event.second.duration))
                            << ",";
                        out << static_cast<unsigned>(event.second.outcome) << ",";
                        out << event.second.number << ",";
                        out << event.second.ops << ",";
                        out << event.second.errors << ",";
                        out << event.second.size << std::endl;

                        logMaybe(++iter, actorName, opName);
                    }
                }
            }
        }
    }

    static bool shouldSkipReporting(const std::string& actorName, const std::string& opName) {
        // The cedar-csv metrics format ignores the Genny.ActorStarted and Genny.ActorFinished
        // operations reported by the DefaultDriver because the OperationThreadCounts section
        // effectively tracks the number of concurrent actors and that number isn't meaningfully
        // changing over time.
        return actorName == "Genny" && (opName == "ActorStarted" || opName == "ActorFinished");
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

    const RegistryT<MetricsClockSource>* const _registry;
};

}  // namespace internals::v1

using Reporter = internals::v1::ReporterT<Registry::clock>;

}  // namespace genny::metrics

#endif  // HEADER_1EB08DF5_3853_4277_8B3D_4542552B8154_INCLUDED
