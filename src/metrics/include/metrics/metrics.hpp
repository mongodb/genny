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

#ifndef HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED
#define HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED

#include <boost/filesystem.hpp>
#include <chrono>
#include <optional>
#include <type_traits>
#include <unordered_map>

#include <gennylib/Node.hpp>
#include <gennylib/conventions.hpp>

#include <metrics/operation.hpp>
#include <metrics/v1/passkey.hpp>


namespace genny::metrics {

// The directory to use for internal operations.
const std::string INTERNAL_DIR = "internal";

/**
 * Class wrapping the logic involving metrics formats.
 */
class MetricsFormat {
public:
    enum class Format {
        kCsv,
        kCedarCsv,
        kFtdc,
        kCsvFtdc,
    };

    MetricsFormat() : _format{Format::kCsv} {}

    MetricsFormat(const Node& node) : _format{strToEnum(node.to<std::string>())} {}

    MetricsFormat(const std::string& toConvert) : _format{strToEnum(toConvert)} {}

    bool useGrpc() const {
        return _format == Format::kFtdc || _format == Format::kCsvFtdc;
    }

    bool useCsv() const {
        return _format == Format::kCsv || _format == Format::kCedarCsv ||
            _format == Format::kCsvFtdc;
    }

    Format get() const {
        return _format;
    }

    [[nodiscard]] bool operator!=(const MetricsFormat& rhs) const {
        return this->_format != rhs._format;
    }

    [[nodiscard]] std::string toString() const {
        switch (this->_format) {
            case Format::kCsv:
                return "csv";
            case Format::kCedarCsv:
                return "cedar-csv";
            case Format::kFtdc:
                return "ftdc";
            case Format::kCsvFtdc:
                return "csv-ftdc";
        }
        BOOST_THROW_EXCEPTION(InvalidConfigurationException("Impossible"));
    }

private:
    Format _format;
    Format strToEnum(const std::string& toConvert) {
        if (toConvert == "csv") {
            return Format::kCsv;
        } else if (toConvert == "cedar-csv") {
            return Format::kCedarCsv;
        } else if (toConvert == "ftdc") {
            return Format::kFtdc;
        } else if (toConvert == "csv-ftdc") {
            return Format::kCsvFtdc;
        } else {
            throw std::invalid_argument(std::string("Unknown metrics format ") + toConvert);
        }
    }
};

/**
 * @namespace genny::metrics::internals this namespace is private and only intended to be used by
 * genny's own internals. No types from the genny::metrics::internals namespace should ever be typed
 * directly into the implementation of an actor.
 */
namespace internals {

template <typename Clocksource>
class OperationImpl;

class MetricsClockSource {
private:
    using clock_type = std::chrono::steady_clock;
    using report_clock_type = std::chrono::system_clock;


public:
    using duration = clock_type::duration;
    using time_point = std::chrono::time_point<clock_type>;
    using report_time_point = std::chrono::time_point<report_clock_type>;

    static time_point now() {
        return clock_type::now();
    }

    /**
     * Translate a given time point to a one suitable for
     * external reporting.
     */
    static report_time_point toReportTime(time_point givenTime) {
        auto timeSinceStarted = givenTime - _timeStarted;
        auto reportDur = std::chrono::duration_cast<report_clock_type::duration>(timeSinceStarted);
        return _reportTimeStarted + reportDur;
    }

private:
    // Inlining lets us initialize these in the header.
    inline static time_point _timeStarted = now();
    inline static report_time_point _reportTimeStarted = report_clock_type::now();
};

/**
 * Supports recording a number of types of Time-Series Values:
 *
 * - Counters:   a count of things that can be incremented or decremented
 * - Gauges:     a "current" number of things; a value that can be known and observed
 * - Timers:     recordings of how long certain operations took
 *
 * All data-points are recorded along with the ClockSource::now() value of when
 * the points are recorded.
 *
 * It is expensive to create a distinct metric name but cheap to record new values.
 * The first time `registry.counter("foo")` is called for a distinct counter
 * name "foo", a large block of memory is reserved to store its data-points. But
 * all calls to `registry.counter("foo")` return pimpl-backed wrappers that are cheap
 * to construct and are safe to pass-by-value. Same applies for other metric types.
 *
 * As of now, none of the metrics classes are thread-safe, however they are all
 * thread-compatible. Two threads may not record values to the same metrics names
 * at the same time.
 *
 * `metrics::Reporter` instances have read-access to the TSD data, but that should
 * only be used by workload-drivers to produce a report of the metrics at specific-points
 * in their workload lifecycle.
 */
template <typename ClockSource>
class RegistryT final {
private:
    using OperationsByThread = std::unordered_map<ActorId, OperationImpl<ClockSource>>;
    using OperationsByType = std::unordered_map<std::string, OperationsByThread>;
    // OperationsMap is a map of
    // actor name -> operation name -> actor id -> OperationImpl (time series).
    using OperationsMap = std::unordered_map<std::string, OperationsByType>;

    using GrpcClient = v2::GrpcClient<ClockSource, v2::StreamInterfaceImpl>;
    // The client owns the stream and we only instantiate if using grpc.
    using StreamPtr = internals::v2::EventStream<ClockSource, v2::StreamInterfaceImpl>*;

public:
    using clock = ClockSource;

    explicit RegistryT() = default;

    explicit RegistryT(bool assertMetricsBuffer) : RegistryT({}, {}, assertMetricsBuffer) {}

    explicit RegistryT(MetricsFormat format,
                       boost::filesystem::path pathPrefix,
                       bool assertMetricsBuffer = true)
        : _format{std::move(format)},
          _pathPrefix{std::move(pathPrefix)},
          _internalPathPrefix{_pathPrefix / INTERNAL_DIR} {
        if (_format.useGrpc()) {
            boost::filesystem::create_directories(_pathPrefix);
            boost::filesystem::create_directories(_internalPathPrefix);
            _grpcClient = std::make_unique<GrpcClient>(assertMetricsBuffer);
        }
    }


    OperationT<ClockSource> operation(std::string actorName,
                                      std::string opName,
                                      ActorId actorId,
                                      std::optional<genny::PhaseNumber> phase = std::nullopt,
                                      bool internal = false) {
        StreamPtr stream = nullptr;

        auto pathPrefix = internal ? _internalPathPrefix : _pathPrefix;
        auto& opsByType = this->_ops[actorName];
        auto& opsByThread = opsByType[opName];
        if (_format.useGrpc() && opsByThread.find(actorId) == opsByThread.end()) {
            auto name = createName(actorName, opName, phase, internal);
            stream = _grpcClient->createStream(actorId, name, phase, pathPrefix);
        }
        auto opIt =
            opsByThread.try_emplace(actorId, std::move(actorName), *this, std::move(opName), stream)
                .first;
        return OperationT{opIt->second};
    }

    OperationT<ClockSource> operation(std::string actorName,
                                      std::string opName,
                                      ActorId actorId,
                                      genny::TimeSpec threshold,
                                      double_t percentage,
                                      std::optional<genny::PhaseNumber> phase = std::nullopt,
                                      bool internal = false) {
        auto& opsByType = this->_ops[actorName];
        auto& opsByThread = opsByType[opName];
        auto pathPrefix = internal ? _internalPathPrefix : _pathPrefix;
        StreamPtr stream = nullptr;
        if (_format.useGrpc() && opsByThread.find(actorId) == opsByThread.end()) {
            auto name = createName(actorName, opName, phase, internal);
            stream = _grpcClient->createStream(actorId, name, phase, pathPrefix);
        }
        auto opIt =
            opsByThread
                .try_emplace(
                    actorId,
                    std::move(actorName),
                    *this,
                    std::move(opName),
                    stream,
                    std::make_optional<typename OperationImpl<ClockSource>::OperationThreshold>(
                        threshold, percentage))
                .first;
        return OperationT{opIt->second};
    }

    [[nodiscard]] const OperationsMap& getOps(v1::Permission) const {
        return this->_ops;
    };

    [[nodiscard]] const typename ClockSource::time_point now(v1::Permission) const {
        return ClockSource::now();
    }

    /**
     * Returns the number of workers performing a given operation.
     * Assumes the count is constant across phases for a given (actor, operation).
     */
    std::size_t getWorkerCount(const std::string& actorName, const std::string& opName) const {
        return (_ops.at(actorName).at(opName)).size();
    }


    const MetricsFormat& getFormat() const {
        return _format;
    }

    const boost::filesystem::path& getPathPrefix() const {
        return _pathPrefix;
    }

private:
    std::string createName(const std::string& actorName,
                           const std::string& opName,
                           const std::optional<genny::PhaseNumber>& phase,
                           bool internal) {
        std::stringstream str;

        // We want the trend graph to be hidden by
        // default to not confuse users, so we prefix it with "canary_" to hit the
        // CANARY_EXCLUSION_REGEX in https://git.io/Jtjdr
        if (internal) {
            str << "canary_";
        }
        str << actorName << '.' << opName;
        if (phase) {
            str << '.' << *phase;
        }
        return str.str();
    }

    std::unique_ptr<GrpcClient> _grpcClient;
    OperationsMap _ops;
    MetricsFormat _format;
    boost::filesystem::path _pathPrefix;
    boost::filesystem::path _internalPathPrefix;
};

}  // namespace internals

using Registry = internals::RegistryT<internals::MetricsClockSource>;

static_assert(std::is_move_constructible<Registry>::value, "move");
static_assert(std::is_move_assignable<Registry>::value, "move");

// Registry::clock is actually the ClockSource template parameter, so we check the underlying
// clock_type in a roundabout way by going through the time_point type it exposes.
static_assert(Registry::clock::time_point::clock::is_steady, "clock must be steady");

using Operation = internals::OperationT<Registry::clock>;
using OperationContext = internals::OperationContextT<Registry::clock>;
using OperationEvent = OperationEventT<Registry::clock>;

// Convenience types
using time_point = Registry::clock::time_point;
using clock = Registry::clock;


}  // namespace genny::metrics

#endif  // HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED
