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

#include <chrono>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <boost/filesystem.hpp>

#include <gennylib/conventions.hpp>

#include <metrics/operation.hpp>
#include <metrics/v1/passkey.hpp>

namespace genny::metrics {

/**
 * Class wrapping the logic involving metrics formats.
 */
class MetricsFormat {
public:
    enum class Format {
        csv = 0,
        cedar_csv = 1,
        ftdc = 2,
        csv_ftdc = 3,
    };

    MetricsFormat() : _format{Format::csv} {}

    MetricsFormat(const std::string& to_convert) : _format{str_to_enum(to_convert)} {}

    bool use_grpc() const { return _format == Format::ftdc || _format == Format::csv_ftdc; }

    bool use_csv() const { return _format == Format::csv || _format == Format::cedar_csv 
        || _format == Format::csv_ftdc; }

    Format get() const {return _format;}

private:
    Format _format;
    Format str_to_enum(const std::string& to_convert) {
        if (to_convert == "csv") {
            return Format::csv;
        } else if (to_convert == "cedar-csv") {
            return Format::cedar_csv;
        } else if (to_convert == "ftdc") {
            return Format::ftdc;
        } else if (to_convert == "csv-ftdc") {
            return Format::csv_ftdc;
        } else {
            throw std::invalid_argument(std::string("Unknown metrics format ") + to_convert);
        }
    }
};

/**
 * @namespace genny::metrics::internals this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::internals namespace should ever be typed directly into
 * the implementation of an actor.
 */
namespace internals {

template <typename Clocksource>
class OperationImpl;

class MetricsClockSource {
private:
    using clock_type = std::chrono::steady_clock;

public:
    using duration = clock_type::duration;
    using time_point = std::chrono::time_point<clock_type>;

    static time_point now() {
        return clock_type::now();
    }
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

public:
    using clock = ClockSource;

    // Can use the default constructor if not using poplar metrics.
    explicit RegistryT() = default;

    /**
     * Constructor for using poplar metrics.
     */
    RegistryT(const MetricsFormat& format, const std::string& path_prefix) : _format{format}, 
        _path_prefix{path_prefix} {
            if (_format.use_grpc()) {
                boost::filesystem::create_directory(path_prefix); 
            }
        }

    OperationT<ClockSource> operation(std::string actorName,
                                      std::string opName,
                                      ActorId actorId,
                                      std::optional<genny::PhaseNumber> phase = std::nullopt) {
        auto& opsByType = this->_ops[actorName];
        auto& opsByThread = opsByType[opName];
        auto opIt =
            opsByThread
                .try_emplace(
                    actorId, actorId, std::move(actorName), *this, std::move(opName), std::move(phase), _path_prefix)
                .first;
        return OperationT{opIt->second};
    }

    OperationT<ClockSource> operation(std::string actorName,
                                      std::string opName,
                                      ActorId actorId,
                                      genny::TimeSpec threshold,
                                      double_t percentage,
                                      std::optional<genny::PhaseNumber> phase = std::nullopt) {
        auto& opsByType = this->_ops[actorName];
        auto& opsByThread = opsByType[opName];
        auto opIt =
            opsByThread
                .try_emplace(
                    actorId,
                    actorId,
                    std::move(actorName),
                    *this,
                    std::move(opName),
                    std::move(phase),
                    _path_prefix,
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

    const MetricsFormat& getFormat() const {return _format;}

private:
    OperationsMap _ops;
    MetricsFormat _format;
    std::string _path_prefix;
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
