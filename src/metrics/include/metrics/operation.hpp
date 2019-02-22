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

#include <string>

#include <gennylib/Actor.hpp>

#include <metrics/Period.hpp>
#include <metrics/TimeSeries.hpp>

namespace genny::metrics {
namespace v1 {

using count_type = long long;

struct OperationDescriptor {
    // TODO: The ActorId is enough to rely on for uniqueness so we probably don't need to include
    // `actorName` or `opName` in the Hasher or operator== implementations actually.
    struct Hasher {
        size_t operator()(const OperationDescriptor& desc) const {
            size_t seed = 0;

            boost::hash_combine(seed, desc.actorId);
            boost::hash_combine(seed, desc.actorName);
            boost::hash_combine(seed, desc.opName);

            return seed;
        }
    };

    OperationDescriptor(ActorId actorId, std::string actorName, std::string opName)
        : actorId(actorId), actorName(std::move(actorName)), opName(std::move(opName)) {}

    bool operator==(const OperationDescriptor& other) const {
        return actorId == other.actorId && actorName == other.actorName && opName == other.opName;
    }

    const ActorId actorId;
    const std::string actorName;
    const std::string opName;
};


template <typename ClockSource>
struct OperationEvent {
    enum class OutcomeType : uint8_t { kSuccess = 0, kFailure = 1, kUnknown = 2 };

    count_type iters = 1;  // # iterations, usually 1
    count_type ops = 0;    // # docs
    count_type size = 0;   // # bytes
    count_type errors = 0;
    Period<ClockSource> duration = {};
    OutcomeType outcome = OutcomeType::kUnknown;
};


template <typename ClockSource>
class OperationImpl {
private:
    using OutcomeType = typename OperationEvent<ClockSource>::OutcomeType;

public:
    using time_point = typename ClockSource::time_point;
    using EventSeries = TimeSeries<ClockSource, OperationEvent<ClockSource>>;

    OperationImpl(OperationDescriptor desc, EventSeries& events)
        : _desc(std::move(desc)), _events(std::addressof(events)) {}

    /**
     * Operation name getter to help with exception reporting.
     */
    const std::string& getOpName() const {
        return _desc.opName;
    }

    void reportNumIterations(count_type total) {
        _curr.iters = total;
    }

    void reportNumOps(count_type total) {
        _curr.ops = total;
    }

    void reportNumBytes(count_type total) {
        _curr.size = total;
    }

    void reportNumErrors(count_type total) {
        _curr.errors = total;
    }

    void reportSuccess(time_point started) {
        reportOutcome(started, OutcomeType::kSuccess);
    }

    void reportFailure(time_point started) {
        reportOutcome(started, OutcomeType::kFailure);
    }

    void discard() {
        _curr = OperationEvent<ClockSource>{};
    }

private:
    void reportOutcome(time_point started, OutcomeType outcome) {
        // TODO: We shouldn't be calling ClockSource::now() again as part of TimeSeries::add().
        _curr.duration = ClockSource::now() - started;
        _curr.outcome = outcome;

        _events->add(std::move(_curr));
        _curr = OperationEvent<ClockSource>{};
    }

    OperationEvent<ClockSource> _curr;
    const OperationDescriptor _desc;
    EventSeries* _events;
};


template <typename ClockSource>
class OperationContextT {
public:
    explicit OperationContextT(v1::OperationImpl<ClockSource>& op)
        : _op{std::addressof(op)}, _started{ClockSource::now()} {}

    OperationContextT(OperationContextT<ClockSource>&& rhs) noexcept
        : _op{std::move(rhs._op)},
          _started{std::move(rhs._started)},
          _isClosed{std::move(rhs._isClosed)},
          _totalBytes{std::move(rhs._totalBytes)},
          _totalOps{std::move(rhs._totalOps)} {
        rhs._isClosed = true;
    }

    ~OperationContextT() {
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
     *
     * TODO: Update this comment and rename it to failure(). We should consider exposing a discard()
     * method if we really don't want to report metrics.
     */
    void fail() {
        _isClosed = true;
    }

private:
    using time_point = typename ClockSource::time_point;

    void report() {
        this->_op->reportSuccess(_started);
        this->_op->reportNumBytes(_totalBytes);
        this->_op->reportNumOps(_totalOps);
    }

    v1::OperationImpl<ClockSource>* _op;
    time_point _started;
    OperationEvent<ClockSource> _curr;
    count_type _totalBytes = 0;
    count_type _totalOps = 0;
    bool _isClosed = false;
};


template <typename ClockSource>
class OperationT {
public:
    explicit OperationT(v1::OperationImpl<ClockSource> op) : _op{std::move(op)} {}

    OperationContextT<ClockSource> start() {
        return OperationContextT<ClockSource>(this->_op);
    }

private:
    v1::OperationImpl<ClockSource> _op;
};

}  // namespace v1
}  // namespace genny::metrics

#endif  // HEADER_3D319F23_C539_4B6B_B4E7_23D23E2DCD52_INCLUDED
