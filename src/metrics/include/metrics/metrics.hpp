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

#include <cassert>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <boost/core/noncopyable.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/Actor.hpp>

#include <metrics/TimeSeries.hpp>
#include <metrics/operation.hpp>
#include <metrics/passkey.hpp>

namespace genny::metrics {

/*
 * Could add a template policy class on Registry to let the following types be templatized.
 *
 * TODO: Remove these type aliases and just have the version defined in Operation.hpp.
 */
using count_type = long long;
using gauged_type = long long;

// The v1 namespace is here for two reasons:
// 1) it's a step towards an ABI. These classes are basically the pimpls of the outer classes.
// 2) it prevents auto-completion of metrics::{X}Impl when you really want metrics::{X}.
//
// Note: Counter, Timer and Guage have been deprecated. Use Operation instead when writing actors.

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by Genny's
 * internals. Actors should never have to type `genny::*::v1` into any types.
 */
namespace v1 {

/**
 * Data-storage backing a `Counter`.
 * Please see the documentation there.
 */
template <typename ClockSource>
class CounterImpl : private boost::noncopyable {

public:
    void reportValue(const count_type& delta) {
        auto nval = (this->_count += delta);
        _timeSeries.add(nval);
    }

    // passkey:
    const TimeSeries<ClockSource, count_type>& getTimeSeries(Permission) const {
        return this->_timeSeries;
    }

private:
    TimeSeries<ClockSource, count_type> _timeSeries;
    count_type _count{};
};


/**
 * Data-storage backing a `Gauge`.
 * Please see the documentation there.
 */
template <typename ClockSource>
class GaugeImpl : private boost::noncopyable {

public:
    void set(const gauged_type& count) {
        _timeSeries.add(count);
    }

    // passkey:
    const TimeSeries<ClockSource, count_type>& getTimeSeries(Permission) const {
        return this->_timeSeries;
    }

private:
    TimeSeries<ClockSource, count_type> _timeSeries;
};


/**
 * Data-storage backing a `Timer`.
 * Please see the documentation there.
 */
template <typename ClockSource>
class TimerImpl : private boost::noncopyable {

public:
    void report(const typename ClockSource::time_point& started) {
        _timeSeries.add(ClockSource::now() - started);
    }

    // passkey:
    const TimeSeries<ClockSource, Period<ClockSource>>& getTimeSeries(Permission) const {
        return this->_timeSeries;
    }

private:
    TimeSeries<ClockSource, Period<ClockSource>> _timeSeries;
};


/**
 * Counter is deprecated in favor of Operation.
 *
 * A Counter lets callers indicate <b>deltas</b> of a value at a particular time.
 * A Counter has an (internal, hidden) current value that can be incremented or
 * decremented over time.
 *
 * This is useful when simply recording the number of operations completed.
 *
 * ```c++
 * // setup:
 * auto requests = registry.counter("requests");
 *
 * // main method
 * while(true) {
 *   requests.incr();
 * }
 * ```
 */
template <typename ClockSource>
class Counter {

public:
    explicit constexpr Counter(v1::CounterImpl<ClockSource>& counter)
        : _counter{std::addressof(counter)} {}

    void incr(const count_type& val = 1) {
        _counter->reportValue(val);
    }
    void decr(const count_type& val = -1) {
        _counter->reportValue(val);
    }

private:
    v1::CounterImpl<ClockSource>* _counter;
};


/**
 * Gauge is deprecated in favor of Operation.
 *
 * A Gauge lets you record a known value. E.g. the number
 * of active sessions, how many threads are waiting on something, etc.
 * It is defined by each metric what the value is interpreted to be
 * between calls to `set()`. E.g.
 *
 * ```cpp
 * sessions.set(3);
 * // do something
 * sessions.set(5);
 * ```
 *
 * How to determine the value for the "do something" time-period
 * needs to be interpreted for each metric individually.
 */
template <typename ClockSource>
class Gauge {

public:
    explicit constexpr Gauge(v1::GaugeImpl<ClockSource>& gauge) : _gauge{std::addressof(gauge)} {}

    void set(const gauged_type& value) {
        _gauge->set(value);
    }

private:
    v1::GaugeImpl<ClockSource>* _gauge;
};


/**
 * A timer object that automatically reports the elapsed
 * time from when it was constructed in its dtor.
 *
 * Example usage:
 *
 * ```cpp
 * // setup:
 * auto timer = registry.timer("loops");
 *
 * // main method:
 * for(int i=0; i<5; ++i) {
 *     auto r = timer.raii();
 * }
 * ```
 *
 * You can call `.report()` multiple times manually
 * but that does not prevent the timer from reporting on
 * its own in its dtor.
 */
template <typename ClockSource>
class RaiiStopwatch {

public:
    explicit RaiiStopwatch(v1::TimerImpl<ClockSource>& timer)
        : _timer{std::addressof(timer)}, _started{ClockSource::now()} {}
    RaiiStopwatch(const RaiiStopwatch<ClockSource>& other) = delete;
    RaiiStopwatch(RaiiStopwatch<ClockSource>&& other) noexcept : _started{other._started} {
        this->_timer = other._timer;
        other._timer = nullptr;
    }

    RaiiStopwatch<ClockSource>& operator=(RaiiStopwatch<ClockSource> other) noexcept = delete;

    ~RaiiStopwatch() {
        if (this->_timer != nullptr) {
            this->report();
        };
    }

    void report() {
        assert(this->_timer);
        this->_timer->report(_started);
    }

private:
    using time_point = typename ClockSource::time_point;

    v1::TimerImpl<ClockSource>* _timer;
    const time_point _started;
};


/**
 * Similar to `RaiiStopwatch` but doesn't automatically
 * report on its own. Records the time at which it was constructed
 * and then emits a metric event every time `.report()` is called.
 *
 * Example usage:
 *
 * ```c++
 * // setup
 * auto oper = registry.timer("operation.success");
 *
 * // main method
 * for(int i=0; i<10; ++i) {
 *     auto t = oper.start();
 *     try {
 *         // do something
 *         t.report();
 *     } catch(...) { ... }
 * }
 * ```
 *
 * The `.report()` is only called in the successful
 * scenarios, not if an exception is thrown.
 */
template <typename ClockSource>
class Stopwatch {

public:
    explicit Stopwatch(v1::TimerImpl<ClockSource>& timer)
        : _timer{std::addressof(timer)}, _started{ClockSource::now()} {}

    void report() {
        this->_timer->report(_started);
    }

private:
    using time_point = typename ClockSource::time_point;

    v1::TimerImpl<ClockSource>* const _timer;
    const time_point _started;
};

/**
 * Timer is deprecated in favor of Operation.
 */
template <typename ClockSource>
class Timer {

public:
    explicit constexpr Timer(v1::TimerImpl<ClockSource>& t) : _timer{std::addressof(t)} {}

    /**
     * @return
     *  a `Stopwatch` instance that must be manually reported via `.report()`.
     *  When calling `.report()`, the amount of time elapsed from the calling of `.start()`
     *  to calling `.report()` is reported to the metrics back-end. Can call `.report()` multiple
     *  times. Use `.start()` when you want to record successful outcomes of some specific
     *  code-path. If you never call `.report()`, no metrics data will be recorded.
     *
     *  Both `Stopwatch` and `RaiiStopwatch` record timing data, and they can share names.
     *  They are simply two APIs for reporting timing data.
     */
    [[nodiscard]] Stopwatch<ClockSource> start() const {
        return Stopwatch<ClockSource>{*_timer};
    }

    /**
     * @return
     *  an `RaiiStopwatch` that will automatically report the time elapsed since it was
     *  constructed in its dtor. Call `.raii()` at the start of your method or scope to
     *  record how long that method or scope takes even in the case of exceptions or early-returns.
     *  You can also manually call `.report()` multiple times, but it's unclear if this is
     *  useful.
     *
     * Both `Stopwatch` and `RaiiStopwatch` record timing data, and they can share names.
     * They are simply two APIs for reporting timing data.
     */
    [[nodiscard]] RaiiStopwatch<ClockSource> raii() const {
        return RaiiStopwatch<ClockSource>{*_timer};
    };

private:
    v1::TimerImpl<ClockSource>* _timer;
};


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
class RegistryT {

public:
    using clock = ClockSource;

    explicit RegistryT() = default;

    v1::Counter<ClockSource> counter(const std::string& name) {
        return v1::Counter<ClockSource>{this->_counters[name]};
    }
    v1::Timer<ClockSource> timer(const std::string& name) {
        return v1::Timer<ClockSource>{this->_timers[name]};
    }
    v1::Gauge<ClockSource> gauge(const std::string& name) {
        return v1::Gauge<ClockSource>{this->_gauges[name]};
    }
    OperationT<ClockSource> operation(ActorId id, std::string actorName, std::string opName) {
        auto desc = OperationDescriptor{id, std::move(actorName), std::move(opName)};
        auto it = this->_ops.try_emplace(desc).first;
        auto op = v1::OperationImpl<ClockSource>{std::move(desc), it->second};
        return OperationT{std::move(op)};
    }

    // passkey:
    const std::unordered_map<std::string, v1::CounterImpl<ClockSource>>& getCounters(
        v1::Permission) const {
        return this->_counters;
    };

    const std::unordered_map<std::string, v1::TimerImpl<ClockSource>>& getTimers(
        v1::Permission) const {
        return this->_timers;
    };

    const std::unordered_map<std::string, v1::GaugeImpl<ClockSource>>& getGauges(
        v1::Permission) const {
        return this->_gauges;
    };

    const typename ClockSource::time_point now(v1::Permission) const {
        return ClockSource::now();
    }

private:
    using EventSeries = typename v1::OperationImpl<ClockSource>::EventSeries;

    std::unordered_map<std::string, v1::CounterImpl<ClockSource>> _counters;
    std::unordered_map<std::string, v1::TimerImpl<ClockSource>> _timers;
    std::unordered_map<std::string, v1::GaugeImpl<ClockSource>> _gauges;
    std::unordered_map<OperationDescriptor, EventSeries, OperationDescriptor::Hasher> _ops;
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
