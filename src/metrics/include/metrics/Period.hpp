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
#include <utility>

namespace genny::metrics {
namespace v1 {

// Convenience (wouldn't want to be configurable in the future)

template <typename ClockSource>
class Period {
public:
    using duration = typename ClockSource::duration;

    Period() = default;

    operator duration() const {
        return _duration;
    }

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

    friend std::ostream& operator<<(std::ostream& os, const Period& period) {
        return os << period._duration.count();
    }

private:
    duration _duration;
};

}  // namespace v1
}  // namespace genny::metrics

#endif  // HEADER_E8439AA0_1871_4F5A_B672_BE8E29AA5ED7_INCLUDED
