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

#ifndef HEADER_9ECECB02_6528_456C_B390_AFBAA5229D3D_INCLUDED
#define HEADER_9ECECB02_6528_456C_B390_AFBAA5229D3D_INCLUDED

#include <utility>
#include <vector>

#include <metrics/passkey.hpp>

namespace genny::metrics {
namespace v1 {

/**
 * Not intended to be used directly.
 * This is used by the *Impl classes as storage for TSD values.
 *
 * @tparam T The value to record at a particular time-point.
 */
template <class ClockSource, class T>
class TimeSeries : private boost::noncopyable {

public:
    using time_point = typename ClockSource::time_point;

    explicit constexpr TimeSeries() {
        // could make 1000*1000 a param passed down from Registry if needed
        _vals.reserve(1000 * 1000);
    }

    /**
     * Add a TSD data point occurring `now()`.
     * Args are forwarded to the `T` constructor.
     */
    template <class... Args>
    void add(Args&&... args) {
        _vals.emplace_back(ClockSource::now(), std::forward<Args>(args)...);
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

}  // namespace v1
}  // namespace genny::metrics

#endif  // HEADER_9ECECB02_6528_456C_B390_AFBAA5229D3D_INCLUDED
