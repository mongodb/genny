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

#ifndef HEADER_3D319F23_C539_4B6B_B4E7_23D23E2DCD52_INCLUDED
#define HEADER_3D319F23_C539_4B6B_B4E7_23D23E2DCD52_INCLUDED

#include <cstdint>
#include <exception>
#include <memory>
#include <ostream>
#include <string>

#include <boost/core/noncopyable.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/Actor.hpp>

#include <metrics/Period.hpp>
#include <metrics/v1/TimeSeries.hpp>

namespace genny::metrics {

using count_type = long long;

enum class OutcomeType : uint8_t { kSuccess = 0, kFailure = 1, kUnknown = 2 };

/**
 * The data captured at a particular time-point.
 *
 * @tparam ClockSource a wrapper type around a std::chrono::steady_clock, should always be
 * MetricsClockSource other than during testing.
 */
template <typename ClockSource>
struct OperationEventT final {

    bool operator==(const OperationEventT<ClockSource>& other) const {
        return iters == other.iters && ops == other.ops && size == other.size &&
            errors == other.errors && duration == other.duration && outcome == other.outcome;
    }

    friend std::ostream& operator<<(std::ostream& out, const OperationEventT<ClockSource>& event) {
        out << "OperationEventT{";
        out << "iters:" << event.iters;
        out << ",ops:" << event.ops;
        out << ",size:" << event.size;
        out << ",errors:" << event.errors;
        out << ",duration:" << event.duration;
        // Casting to uint8_t directly causes ostream to use its unsigned char overload and treat
        // the value as invisible text.
        out << ",outcome:" << static_cast<unsigned>(event.outcome);
        out << "}";
        return out;
    }

    /**
     * @param iters
     *     The number of iterations that occurred before the operation was reported. This member
     * will almost always be 1 unless an actor decides to periodically report an operation in its
     * for loop.
     * @param ops
     *      The number of documents inserted, modified, deleted, etc.
     * @param size
     *      The size in bytes of the number of documents inserted, etc.
     * @param errors
     *      The number of write errors, transient transaction errors, etc. that occurred when
     * performing the operation. The operation can still be considered OutcomeType::kSuccess even if
     * errors are reported.
     * @param duration
     *      The amount of time it took to perform the operation.
     * @param outcome
     *      Whether the operation succeeded.
     */
    explicit OperationEventT(count_type iters = 0,
                             count_type ops = 0,
                             count_type size = 0,
                             count_type errors = 0,
                             Period<ClockSource> duration = {},
                             OutcomeType outcome = OutcomeType::kUnknown)
        : iters{iters}, ops{ops}, size{size}, errors{errors}, duration{duration} {}

    count_type iters;              // corresponds to the 'n' field in Cedar
    count_type ops;                // corresponds to the 'ops' field in Cedar
    count_type size;               // corresponds to the 'size' field in Cedar
    count_type errors;             // corresponds to the 'errors' field in Cedar
    Period<ClockSource> duration;  // corresponds to the 'duration' field in Cedar
    OutcomeType outcome;           // corresponds to the 'outcome' field in Cedar
};

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v1 namespace should ever be typed directly into
 * the implementation of an actor.
 */
namespace v1 {

/**
 * Throw this to indicate the percentage of operations exceeding the
 * time limit went above the threshold.
 *
 * Intended to be used alongside OperationImpl::OperationThreshold
 */
class OperationThresholdExceededException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template <typename ClockSource>
class OperationImpl final : private boost::noncopyable {
private:  // Data members.
    struct OperationCount {
        int64_t failed = 0;
        int64_t total = 0;

        constexpr double_t failedPercentage() const {
            return static_cast<double_t>(failed) / total * 100;
        };
    };

public:
    using time_point = typename ClockSource::time_point;
    using EventSeries = TimeSeries<ClockSource, OperationEventT<ClockSource>>;

    struct OperationThreshold {
        std::chrono::nanoseconds maxDuration;
        double_t maxPercentAllowedToExceed;
        OperationCount opCounter;

        OperationThreshold(std::chrono::nanoseconds maxDuration, double_t failedPct)
            : maxDuration(maxDuration), maxPercentAllowedToExceed(failedPct), opCounter() {}

        void check(time_point started, time_point finished) {
            opCounter.total++;
            if ((finished - started) > maxDuration) {
                opCounter.failed++;
            }

            if (opCounter.failedPercentage() > maxPercentAllowedToExceed) {
                std::ostringstream os;
                os << opCounter.failedPercentage()
                   << "% of operations failed, exceeding the threshold of "
                   << maxPercentAllowedToExceed << "%";
                BOOST_THROW_EXCEPTION(OperationThresholdExceededException(os.str()));
            }
        }
    };

    using OptionalOperationThreshold = std::optional<OperationThreshold>;

    OperationImpl(std::string actorName,
                  std::string opName,
                  std::optional<OperationThreshold> threshold = std::nullopt)
        : _actorName(std::move(actorName)), _opName(std::move(opName)), _threshold(threshold){};

    /**
     * @return the name of the actor running the operation.
     */
    const std::string& getActorName() const {
        return _actorName;
    }

    /**
     * @return the name of the operation being run.
     */
    const std::string& getOpName() const {
        return _opName;
    }

    /**
     * @return the time series for the operation being run.
     */
    const EventSeries& getEvents() const {
        return _events;
    }

    void reportAt(time_point started, time_point finished, OperationEventT<ClockSource>&& event) {
        if (_threshold) {
            _threshold->check(started, finished);
        }
        _events.addAt(finished, event);
    }

    void reportSynthetic(time_point finished,
                         std::chrono::microseconds duration,
                         count_type iters,
                         count_type ops,
                         count_type size,
                         count_type errors,
                         OutcomeType outcome = OutcomeType::kUnknown) {
        auto started = finished - duration;
        this->reportAt(started, finished, OperationEventT<ClockSource>{
                iters, ops, size, errors,
                Period<ClockSource>{duration},
                outcome
        });
    }

private:
    const std::string _actorName;
    const std::string _opName;
    OptionalOperationThreshold _threshold;
    EventSeries _events;
};

/**
 * RAII type for managing data captured about a running operation. Constructing an instance starts a
 * timer for how long the operation takes. The timer ends when success() or failure() is called on
 * the instance.
 */
template <typename ClockSource>
class OperationContextT final : private boost::noncopyable {
public:
    using time_point = typename ClockSource::time_point;

    explicit OperationContextT(v1::OperationImpl<ClockSource>* op)
        : _op{op}, _started{ClockSource::now()} {}

    OperationContextT(OperationContextT<ClockSource>&& other) noexcept
        : _op{std::move(other._op)},
          _started{std::move(other._started)},
          _event{std::move(other._event)},
          _isClosed{std::exchange(other._isClosed, true)} {}

    ~OperationContextT() {
        if (!_isClosed) {
            BOOST_LOG_TRIVIAL(error)
                << "Metrics not reported because operation '" << _op->getOpName()
                << "' being run by actor '" << _op->getActorName()
                << "' did not close with success() or failure().";
        }
    }

    /**
     * Increments the counter for the number of iterations. This method only needs to be called if
     * an actor is periodically reporting its operations. By default an `iters = 1` value is
     * automatically reported.
     */
    void addIterations(count_type iters) {
        _event.iters += iters;
    }

    /**
     * Increments the counter for the number of documents inserted, modified, deleted, etc.
     */
    void addDocuments(count_type ops) {
        _event.ops += ops;
    }

    /**
     * Increments the size in bytes of the number of documents inserted, etc.
     */
    void addBytes(count_type size) {
        _event.size += size;
    }

    /**
     * Increments the counter for the number of write errors, transient transaction errors, etc.
     */
    void addErrors(count_type errors) {
        _event.errors += errors;
    }

    /**
     * Report the operation as having succeeded.
     *
     * @warning After calling success() it is illegal to call any methods on this instance.
     */
    void success() {
        reportOutcome(OutcomeType::kSuccess);
    }

    /**
     * Report the operation as having failed. An `errors > 0` value won't be reported unless
     * addErrors() has already been called.
     *
     * @warning After calling failure() it is illegal to call any methods on this instance.
     */
    void failure() {
        reportOutcome(OutcomeType::kFailure);
    }

    /**
     * Don't report the operation.
     *
     * @warning After calling discard() it is illegal to call any methods on this instance.
     */
    void discard() {
        _isClosed = true;
    }

private:
    void reportOutcome(OutcomeType outcome) {
        auto finished = ClockSource::now();
        _event.duration = finished - _started;
        _event.outcome = outcome;

        if (_event.iters == 0) {
            // We default the event to represent a single iteration of a loop if addIterations() was
            // never called.
            _event.iters = 1;
        }

        _op->reportAt(_started, finished, std::move(_event));
        _isClosed = true;
    }

    v1::OperationImpl<ClockSource>* const _op;
    const time_point _started;

    OperationEventT<ClockSource> _event;
    bool _isClosed = false;
};


template <typename ClockSource>
class OperationT final {
    using time_point = typename ClockSource::time_point;

public:
    explicit OperationT(v1::OperationImpl<ClockSource>& op) : _op{std::addressof(op)} {}

    OperationContextT<ClockSource> start() {
        return OperationContextT<ClockSource>{this->_op};
    }


    /**
     * Directly record a metrics event.
     * MOST CALLERS SHOULD USE start(). Use this only if the start/end times of your
     * operation are calculated or done in different threads.
     *
     * Example Usage:
     *
     * ```c++
     * auto started = metrics::clock::now();
     * ...
     * auto end = metrics::clock::now();
     * operation.report(
     *     end,
     *     end - started,
     *     iters,
     *     ops,
     *     size,
     *     errors,
     *     outcome
     *  });
     * ```
     *
     * @param finished
     *   when the operation finished. This will be used as the time point the event occurred and
     *   `finished - duration` will be used as when the event started.
     * @param iters
     *     The number of iterations that occurred before the operation was reported. This member
     * will almost always be 1 unless an actor decides to periodically report an operation in its
     * for loop.
     * @param ops
     *   The number of documents inserted, modified, deleted, etc.
     * @param size
     *   The size in
     * bytes of the number of documents inserted, etc.
     * @param errors
     *    The number of write errors, transient
     * transaction errors, etc. that occurred when performing the operation. The operation can still
     * be considered OutcomeType::kSuccess even if errors are reported.
     * @param duration
     *   The amount of time
     * it took to perform the operation.
     * @param outcome
     *   Whether the operation succeeded.
     */
    void report(time_point finished,
                std::chrono::microseconds duration,
                count_type iters = 0,
                count_type ops = 0,
                count_type size = 0,
                count_type errors = 0,
                OutcomeType outcome = OutcomeType::kUnknown) {
        _op->reportSynthetic(finished, duration, iters, ops, size, errors, outcome);
    }


private:
    v1::OperationImpl<ClockSource>* _op;
};

}  // namespace v1
}  // namespace genny::metrics

#endif  // HEADER_3D319F23_C539_4B6B_B4E7_23D23E2DCD52_INCLUDED
