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
#include <type_traits>
#include <unordered_map>

#include <metrics/operation.hpp>
#include <metrics/passkey.hpp>

namespace genny::metrics {

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v1 namespace should ever be typed directly into
 * the implementation of an actor.
 */
namespace v1 {

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

    explicit RegistryT() = default;

    OperationT<ClockSource> operation(std::string actorName, std::string opName, ActorId actorId) {
        auto& opsByType = this->_ops[actorName];
        auto& opsByThread = opsByType[opName];
        auto opIt = opsByThread.try_emplace(actorId, std::move(actorName), std::move(opName)).first;
        return OperationT{opIt->second};
    }

    const OperationsMap& getOps(Permission) const {
        return this->_ops;
    };

    const typename ClockSource::time_point now(Permission) const {
        return ClockSource::now();
    }

private:
    OperationsMap _ops;
};

}  // namespace v1

using Registry = v1::RegistryT<v1::MetricsClockSource>;

static_assert(std::is_move_constructible<Registry>::value, "move");
static_assert(std::is_move_assignable<Registry>::value, "move");

// Registry::clock is actually the ClockSource template parameter, so we check the underlying
// clock_type in a roundabout way by going through the time_point type it exposes.
static_assert(Registry::clock::time_point::clock::is_steady, "clock must be steady");

using Operation = v1::OperationT<Registry::clock>;
using OperationContext = v1::OperationContextT<Registry::clock>;

}  // namespace genny::metrics

#endif  // HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED
