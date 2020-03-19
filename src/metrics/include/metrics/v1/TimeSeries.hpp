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

#include <cstdint>
#include <utility>
#include <vector>

#include <boost/core/noncopyable.hpp>

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v1 namespace should ever be typed directly into
 * the implementation of an actor.
 */
namespace genny::metrics::internals::v1 {

/**
 * A class for storing time series data (TSD) values.
 *
 * @tparam ClockSource a wrapper type around a std::chrono::steady_clock, should always be
 * MetricsClockSource other than during testing.
 *
 * @tparam T the type of value to record at a particular time-point.
 */
template <class ClockSource, class T>
class TimeSeries final : private boost::noncopyable {
public:
    using time_point = typename ClockSource::time_point;
    using ElementType = std::pair<time_point, T>;
    using VectorType = std::vector<ElementType>;

    explicit constexpr TimeSeries() {
        // could make 1000*1000 a param passed down from Registry if needed
        _vals.reserve(1000 * 1000);
    }

    /**
     * Add a TSD data point occurring at `when`.
     * Args are forwarded to the `T` constructor.
     */
    template <class... Args>
    void addAt(time_point when, Args&&... args) {
        _vals.emplace_back(when, std::forward<Args>(args)...);
    }

    const ElementType& operator[](size_t pos) const {
        return _vals[pos];
    }

    size_t size() const {
        return _vals.size();
    }

    typename VectorType::const_iterator begin() const {
        return _vals.cbegin();
    }

    typename VectorType::const_iterator end() const {
        return _vals.cend();
    }

private:
    VectorType _vals;
};

}  // namespace genny::metrics::internals::v1

#endif  // HEADER_9ECECB02_6528_456C_B390_AFBAA5229D3D_INCLUDED
