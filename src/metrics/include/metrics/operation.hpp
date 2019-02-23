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
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include <boost/core/noncopyable.hpp>
#include <boost/log/trivial.hpp>

#include <gennylib/Actor.hpp>

#include <metrics/Period.hpp>
#include <metrics/TimeSeries.hpp>

namespace genny::metrics {

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v1 namespace should ever be typed directly into
 * the implementation of an actor.
 */
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

    bool operator==(const OperationEvent<ClockSource>& other) const {
        return iters == other.iters && ops == other.ops && size == other.size &&
            errors == other.errors && duration == other.duration && outcome == other.outcome;
    }

    count_type iters = 0;  // # iterations, usually 1
    count_type ops = 0;    // # docs
    count_type size = 0;   // # bytes
    count_type errors = 0;
    Period<ClockSource> duration = {};
    OutcomeType outcome = OutcomeType::kUnknown;
};


template <typename ClockSource>
std::ostream& operator<<(std::ostream& out, const OperationEvent<ClockSource>& event) {
    out << "OperationEvent{ ";
    out << "iters: " << event.iters;
    out << ", ops: " << event.ops;
    out << ", size: " << event.size;
    out << ", errors: " << event.errors;
    out << ", duration: " << event.duration;
    // Casting to uint8_t causes ostream to use its unsigned char overload and treat the value as
    // invisible text.
    out << ", outcome: " << static_cast<uint16_t>(event.outcome);
    out << " }";
    return out;
}


template <typename ClockSource>
class OperationImpl {
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

    void reportAt(time_point finished, OperationEvent<ClockSource>&& event) {
        _events->addAt(finished, event);
    }

private:
    const OperationDescriptor _desc;
    EventSeries* _events;
};


template <typename ClockSource>
class OperationContextT : private boost::noncopyable {
private:
    using OutcomeType = typename OperationEvent<ClockSource>::OutcomeType;

public:
    using time_point = typename ClockSource::time_point;

    explicit OperationContextT(v1::OperationImpl<ClockSource>& op)
        : _op{std::addressof(op)}, _started{ClockSource::now()} {}

    OperationContextT(OperationContextT<ClockSource>&& other) noexcept
        : _op{std::move(other._op)},
          _started{std::move(other._started)},
          _event{std::move(other._event)},
          _isClosed{std::exchange(other._isClosed, true)} {}

    ~OperationContextT() {
        if (!_isClosed) {
            BOOST_LOG_TRIVIAL(warning)
                << "Metrics not reported because operation '" << _op->getOpName()
                << "' did not close with success() or failure().";
        }
    }

    void addIterations(count_type iters) {
        _event.iters += iters;
    }

    void addDocuments(count_type ops) {
        _event.ops += ops;
    }

    void addBytes(count_type size) {
        _event.size += size;
    }

    void addErrors(count_type errors) {
        _event.errors += errors;
    }

    void success() {
        reportOutcome(OutcomeType::kSuccess);
    }

    void failure() {
        reportOutcome(OutcomeType::kFailure);
    }

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

        _op->reportAt(finished, std::move(_event));
        _isClosed = true;
    }

    v1::OperationImpl<ClockSource>* const _op;
    const time_point _started;

    OperationEvent<ClockSource> _event;
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
