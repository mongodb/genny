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

#ifndef HEADER_1112A012_F4B7_4C67_9141_7B1179B788CC_INCLUDED
#define HEADER_1112A012_F4B7_4C67_9141_7B1179B788CC_INCLUDED

#include <chrono>

#include <metrics/Period.hpp>

namespace genny::testing {
using namespace std::literals::chrono_literals;

// The template is to allow different DummyClocks classes to be created so calling static methods
// won't conflict.
template <typename DummyT>
/** @private */
struct DummyClock {
    static int64_t nowRaw;

    // <clock-concept>
    using rep = int64_t;
    using period = std::nano;
    using duration = std::chrono::duration<DummyClock::rep, DummyClock::period>;
    using time_point = std::chrono::time_point<DummyClock>;
    const static bool is_steady = true;
    // </clock-concept>

    static auto now() {
        return DummyClock::time_point(DummyClock::duration(nowRaw));
    }
};

template <typename DummyT>
int64_t DummyClock<DummyT>::nowRaw = 0;

class RegistryClockSourceStub {
private:
    using clock_type = std::chrono::steady_clock;

public:
    using duration = clock_type::duration;
    using time_point = std::chrono::time_point<clock_type>;

    static void advance(metrics::v1::Period<clock_type> inc = 1ns) {
        _now += inc;
    }

    static void reset() {
        _now = {};
    }

    static time_point now() {
        return _now;
    }

private:
    static time_point _now;
};

struct ReporterClockSourceStub {
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    static time_point now() {
        return time_point{42ms};
    }
};
}  // namespace genny::testing


#endif  // HEADER_1112A012_F4B7_4C67_9141_7B1179B788CC_INCLUDED
