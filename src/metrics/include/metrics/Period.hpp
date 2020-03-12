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

#ifndef HEADER_E8439AA0_1871_4F5A_B672_BE8E29AA5ED7_INCLUDED
#define HEADER_E8439AA0_1871_4F5A_B672_BE8E29AA5ED7_INCLUDED

#include <ostream>
#include <type_traits>
#include <utility>

namespace genny::metrics {

const auto NANO_PER_SECOND = 1000000000;

/**
 * A wrapper class around std::chrono::duration. It implements operator<<(std::ostream& os) for
 * convenience during testing.
 *
 * @tparam ClockSource a wrapper type around a std::chrono::steady_clock, should always be
 * MetricsClockSource other than during testing.
 */
template <typename ClockSource>
class Period final {
public:
    using duration = typename ClockSource::duration;

    Period() = default;

    // recursive case
    template <typename Arg0, typename... Args>
    Period(Arg0 arg0, Args&&... args)
        : _duration(std::forward<Arg0>(arg0), std::forward<Args>(args)...) {}

    // base-case for arg that is implicitly-convertible to ClockSource::duration
    template <
        typename Arg,
        typename = typename std::enable_if<std::is_convertible<Arg, duration>::value, void>::type>
    Period(Arg&& arg) : _duration(std::forward<Arg>(arg)) {}

    // base-case for arg that isn't explicitly-convertible to ClockSource::duration; marked explicit
    template <
        typename Arg,
        typename = typename std::enable_if<!std::is_convertible<Arg, duration>::value, void>::type,
        typename = void>
    explicit Period(Arg&& arg) : _duration(std::forward<Arg>(arg)) {}

    operator duration() const {
        return _duration;
    }

    bool operator==(const Period<ClockSource>& other) const {
        return _duration == other._duration;
    }

    friend std::ostream& operator<<(std::ostream& os, const Period& period) {
        return os << period._duration.count();
    }

    std::chrono::seconds::rep getSecondsCount() const {
        return std::chrono::duration_cast<std::chrono::seconds>(_duration).count();
    }

    /**
     * Gets the nanosecond component that wouldn't be counted in seconds.
     */
    std::chrono::nanoseconds::rep getNanosecondsCount() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(_duration).count() -
            (getSecondsCount() * NANO_PER_SECOND);
    }


private:
    duration _duration;
};

}  // namespace genny::metrics

#endif  // HEADER_E8439AA0_1871_4F5A_B672_BE8E29AA5ED7_INCLUDED
