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

namespace genny::metrics {

/**
 * The Reporter is given read-access to metrics data for the purposes
 * of reporting data. The Reporter class is the only "non-header"/separately-compiled
 * component of the metrics library. It is not ABI safe.
 */
class Reporter;


/*
 * Could add a template policy class on Registry to let the following types be templatized.
 */
using clock = std::chrono::steady_clock;
using count_type = long long;
using gauged_type = long long;

static_assert(clock::is_steady, "clock must be steady");

// Convenience (wouldn't want to be configurable in the future)

class period {
private:
    clock::duration duration;

public:
    period() = default;

    operator clock::duration() const {
        return duration;
    }

    // recursive case
    template <typename Arg0, typename... Args>
    period(Arg0 arg0, Args&&... args)
        : duration(std::forward<Arg0>(arg0), std::forward<Args>(args)...) {}

    // base-case for arg that is implicitly-convertible to clock::duration
    template <typename Arg,
              typename = typename std::enable_if<std::is_convertible<Arg, clock::duration>::value,
                                                 void>::type>
    period(Arg&& arg) : duration(std::forward<Arg>(arg)) {}

    // base-case for arg that isn't explicitly-convertible to clock::duration; marked explicit
    template <typename Arg,
              typename = typename std::enable_if<!std::is_convertible<Arg, clock::duration>::value,
                                                 void>::type,
              typename = void>
    explicit period(Arg&& arg) : duration(std::forward<Arg>(arg)) {}

    friend std::ostream& operator<<(std::ostream& os, const period& p) {
        return os << p.duration.count();
    }
};

using time_point = std::chrono::time_point<clock>;
using duration_at_time = std::pair<time_point, period>;
using count_at_time = std::pair<time_point, count_type>;
using gauged_at_time = std::pair<time_point, gauged_type>;

class Reporter;
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
 * Ignore this. Used for passkey for some methods.
 */
class Evil {
protected:
    Evil() = default;
};
class Permission : private Evil {

private:
    constexpr Permission() = default;
    // template <typename T>
    friend class genny::metrics::Reporter;
};

static_assert(std::is_empty<Permission>::value, "empty");


/**
 * Not intended to be used directly.
 * This is used by the *Impl classes as storage for TSD values.
 *
 * @tparam T The value to record at a particular time-point.
 */
template <class T>
class TimeSeries : private boost::noncopyable {

public:
    explicit constexpr TimeSeries() {
        // could make 10000*10000 a param passed down
        // from Registry if needed
        _vals.reserve(1000 * 1000);
    }

    /**
     * Add a TSD data point occurring `now()`.
     * Args are forwarded to the `T` constructor.
     */
    template <class... Args>
    void add(Args&&... args) {
        _vals.emplace_back(metrics::clock::now(), std::forward<Args>(args)...);
    }

    // passkey:
    /**
     * Internal method to expose data-points for reporting, etc.
     * @return raw data
     */
    const std::vector<std::pair<time_point, T>>& getVals(Permission) const {
        return _vals;
    }

    /**
     * @return number of data points
     */
    long getDataPointCount(Permission) const {
        return std::distance(_vals.begin(), _vals.end());
    }

private:
    std::vector<std::pair<time_point, T>> _vals;
};


/**
 * Data-storage backing a `Counter`.
 * Please see the documentation there.
 */
class CounterImpl : private boost::noncopyable {

public:
    void reportValue(const count_type& delta) {
        auto nval = (this->_count += delta);
        _timeSeries.add(nval);
    }

    // passkey:
    const TimeSeries<count_type>& getTimeSeries(Permission) const {
        return this->_timeSeries;
    }

private:
    TimeSeries<count_type> _timeSeries;
    count_type _count{};
};


/**
 * Data-storage backing a `Gauge`.
 * Please see the documentation there.
 */
class GaugeImpl : private boost::noncopyable {

public:
    void set(const gauged_type& count) {
        _timeSeries.add(count);
    }

    // passkey:
    const TimeSeries<count_type>& getTimeSeries(Permission) const {
        return this->_timeSeries;
    }

private:
    TimeSeries<count_type> _timeSeries;
};


/**
 * Data-storage backing a `Timer`.
 * Please see the documentation there.
 */
class TimerImpl : private boost::noncopyable {

public:
    void report(const time_point& started) {
        _timeSeries.add(metrics::clock::now() - started);
    }

    // passkey:
    const TimeSeries<period>& getTimeSeries(Permission) const {
        return this->_timeSeries;
    }

private:
    TimeSeries<period> _timeSeries;
};


class OperationImpl {

public:
    OperationImpl(const std::string& name,
                  TimerImpl& timer,
                  CounterImpl& iters,
                  CounterImpl& docs,
                  CounterImpl& bytes)
        : _opName{name},
          _timer{std::addressof(timer)},
          _iters{std::addressof(iters)},
          _docs{std::addressof(docs)},
          _bytes{std::addressof(bytes)} {}

    /**
     * Operation name getter to help with exception reporting.
     */
    const std::string& getOpName() const {
        return _opName;
    }


    void report(const time_point& started) {
        this->_timer->report(started);
        this->_iters->reportValue(1);
    }

    void reportBytes(const count_type& total) {
        this->_bytes->reportValue(total);
    }

    void reportOps(const count_type& total) {
        this->_docs->reportValue(total);
    }


private:
    const std::string _opName;
    TimerImpl* _timer;
    CounterImpl* _iters;
    CounterImpl* _docs;
    CounterImpl* _bytes;
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
class Counter {

public:
    explicit constexpr Counter(v1::CounterImpl& counter) : _counter{std::addressof(counter)} {}

    void incr(const count_type& val = 1) {
        _counter->reportValue(val);
    }
    void decr(const count_type& val = -1) {
        _counter->reportValue(val);
    }

private:
    v1::CounterImpl* _counter;
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
class Gauge {

public:
    explicit constexpr Gauge(v1::GaugeImpl& gauge) : _gauge{std::addressof(gauge)} {}

    void set(const gauged_type& value) {
        _gauge->set(value);
    }

private:
    v1::GaugeImpl* _gauge;
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
class RaiiStopwatch {

public:
    explicit RaiiStopwatch(v1::TimerImpl& timer)
        : _timer{std::addressof(timer)}, _started{metrics::clock::now()} {}
    RaiiStopwatch(const RaiiStopwatch& other) = delete;
    RaiiStopwatch(RaiiStopwatch&& other) noexcept : _started{other._started} {
        this->_timer = other._timer;
        other._timer = nullptr;
    }

    RaiiStopwatch& operator=(RaiiStopwatch other) noexcept = delete;

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
    v1::TimerImpl* _timer;
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
class Stopwatch {

public:
    explicit Stopwatch(v1::TimerImpl& timer)
        : _timer{std::addressof(timer)}, _started{metrics::clock::now()} {}

    void report() {
        this->_timer->report(_started);
    }

private:
    v1::TimerImpl* const _timer;
    const time_point _started;
};

/**
 * Timer is deprecated in favor of Operation.
 */
class Timer {

public:
    explicit constexpr Timer(v1::TimerImpl& t) : _timer{std::addressof(t)} {}

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
    [[nodiscard]] Stopwatch start() const {
        return Stopwatch{*_timer};
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
    [[nodiscard]] RaiiStopwatch raii() const {
        return RaiiStopwatch{*_timer};
    };

private:
    v1::TimerImpl* _timer;
};

}  // namespace v1

class OperationContext {

public:
    explicit OperationContext(v1::OperationImpl& op)
        : _op{std::addressof(op)}, _started{metrics::clock::now()} {}

    OperationContext(OperationContext&& rhs) noexcept
        : _op{std::move(rhs._op)},
          _started{std::move(rhs._started)},
          _isClosed{std::move(rhs._isClosed)},
          _totalBytes{std::move(rhs._totalBytes)},
          _totalOps{std::move(rhs._totalOps)} {
        rhs._isClosed = true;
    }

    ~OperationContext() {
        if (!_isClosed) {
            BOOST_LOG_TRIVIAL(warning)
                << "Metrics not reported because operation '" << this->_op->getOpName()
                << "' did not close with success() or fail().";
        }
    }

    void addBytes(const count_type& size) {
        _totalBytes += size;
    }

    void addOps(const count_type& size) {
        _totalOps += size;
    }

    void success() {
        this->report();
        _isClosed = true;
    }

    /**
     * An operation does not report metrics upon failure.
     */
    void fail() {
        _isClosed = true;
    }

private:
    void report() {
        this->_op->report(_started);
        this->_op->reportBytes(_totalBytes);
        this->_op->reportOps(_totalOps);
    }

    v1::OperationImpl* _op;
    time_point _started;
    count_type _totalBytes = 0;
    count_type _totalOps = 0;
    bool _isClosed = false;
};


class Operation {

public:
    explicit Operation(v1::OperationImpl op) : _op{std::move(op)} {}

    OperationContext start() {
        return OperationContext(this->_op);
    }

private:
    v1::OperationImpl _op;
};


/**
 * Supports recording a number of types of Time-Series Values:
 *
 * - Counters:   a count of things that can be incremented or decremented
 * - Gauges:     a "current" number of things; a value that can be known and observed
 * - Timers:     recordings of how long certain operations took
 *
 * All data-points are recorded along with the clock::now() value of when
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
class Registry {

public:
    explicit Registry() = default;

    v1::Counter counter(const std::string& name) {
        return v1::Counter{this->_counters[name]};
    }
    v1::Timer timer(const std::string& name) {
        return v1::Timer{this->_timers[name]};
    }
    v1::Gauge gauge(const std::string& name) {
        return v1::Gauge{this->_gauges[name]};
    }
    Operation operation(const std::string& name) {
        auto op = v1::OperationImpl(name,
                                    this->_timers[name + "_timer"],
                                    this->_counters[name + "_iters"],
                                    this->_counters[name + "_docs"],
                                    this->_counters[name + "_bytes"]);
        return Operation{std::move(op)};
    }

    // passkey:
    const std::unordered_map<std::string, v1::CounterImpl>& getCounters(v1::Permission) const {
        return this->_counters;
    };

    const std::unordered_map<std::string, v1::TimerImpl>& getTimers(v1::Permission) const {
        return this->_timers;
    };

    const std::unordered_map<std::string, v1::GaugeImpl>& getGauges(v1::Permission) const {
        return this->_gauges;
    };

    const time_point now(v1::Permission) const {
        return metrics::clock::now();
    }

private:
    std::unordered_map<std::string, v1::CounterImpl> _counters;
    std::unordered_map<std::string, v1::TimerImpl> _timers;
    std::unordered_map<std::string, v1::GaugeImpl> _gauges;
};

static_assert(std::is_move_constructible<Registry>::value, "move");
static_assert(std::is_move_assignable<Registry>::value, "move");


}  // namespace genny::metrics

#endif  // HEADER_058638D3_7069_42DC_809F_5DB533FCFBA3_INCLUDED
