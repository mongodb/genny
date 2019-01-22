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

#ifndef HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED
#define HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED

#include <algorithm>
#include <atomic>
#include <chrono>

#include <gennylib/conventions.hpp>

namespace genny::v1 {

/**
 * Rate limiter that applies globally across all threads using the token
 * bucket algorithm.
 *
 * Use GlobalRateLimiter to rate limit from the perspective of the
 * testing target (e.g. MongoDB server). Use RateLimiter to
 * control the schedule of each individual thread.
 *
 * Despite the naming similarities, there should be distinct
 * use case for each class. If you're unsure and "just need
 * a rate limiter", use this one.
 *
 * Notes
 * 1. There can be multiple global rate limiters each responsible for
 * a subset of threads. Coordinating across multiple global rate limiters
 * is currently not supported.
 *
 * 2. The caller is expected to be "nice" and respect the burst size. The
 * rate limiter itself does not know anything about the rate-limited functionality
 * or enforce any behavior about it. This is different to the non-global RateLimiter,
 * which invokes the rate-limited callback function itself.
 *
 * Inspired by
 * https://github.com/facebook/folly/blob/7c6897aa18e71964e097fc238c93b3efa98b2c61/folly/TokenBucket.h
 */
template <typename ClockT>
class BaseGlobalRateLimiter {
public:
    // This should be replaced with std::hardware_destructive_interference_size, which is
    // part of c++17 but not part of any (major) standard library. Search P0154R1
    // for more information.
    //
    // 64 is the cache line size for recent Intel and AMD processors.
    static const int CacheLineSize = 64;

    static_assert(ClockT::is_steady, "Clock must be steady");

private:
    auto now() const {
        const auto nowTime = ClockT::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(nowTime).count();
    }

public:
    explicit BaseGlobalRateLimiter(const RateSpec& rs)
        : _burstSize(rs.operations), _rateNS(rs.per.count()){};

    // No copies or moves.
    BaseGlobalRateLimiter(const BaseGlobalRateLimiter& other) = delete;
    BaseGlobalRateLimiter& operator=(const BaseGlobalRateLimiter& other) = delete;

    BaseGlobalRateLimiter(BaseGlobalRateLimiter&& other) = delete;
    BaseGlobalRateLimiter& operator=(BaseGlobalRateLimiter&& other) = delete;

    ~BaseGlobalRateLimiter() = default;

    /**
     * Request to consume _burstSize number of tokens from the bucket. Does not block
     * if the bucket is empty; does block while waiting for concurrent accesses to the
     * bucket to finish.
     *
     * @return bool whether consume() succeeded. The caller is responsible for using an
     * appropriate back-off strategy if this function returns false.
     */
    bool consume() {
        auto startTime = now();

        // The time the bucket was emptied before this consume() call.
        auto curEmptiedTime = _emptiedTimeNS.load();

        // Check the sentinel value to see this is the first time consume() is called.
        // If so, set the emptied time to be just recent enough to allow one consume()
        // to go through.
        if (curEmptiedTime == LLONG_MAX) {
            curEmptiedTime = startTime - _rateNS;
        }

        // The time the bucket was emptied after this consume() call.
        int64_t newEmptiedTime;
        do {
            newEmptiedTime = curEmptiedTime + _rateNS;

            // If the new emptied time is in the future, the bucket is empty. Return early.
            if (startTime < newEmptiedTime) {
                return false;
            }
        } while (!_emptiedTimeNS.compare_exchange_weak(curEmptiedTime, newEmptiedTime));

        return true;
    }

    constexpr int64_t getBurstSize() const {
        return _burstSize;
    }

    constexpr int64_t getRate() const {
        return _rateNS;
    }

private:
    // Manually align _emptiedTimeNS here to vastly improve performance.
    // Default to an empty bucket to be populated lazily by the first call to consume().
    alignas(BaseGlobalRateLimiter::CacheLineSize) std::atomic<int64_t> _emptiedTimeNS{LLONG_MAX};
    const int64_t _burstSize;
    const int64_t _rateNS;
};

using GlobalRateLimiter = BaseGlobalRateLimiter<std::chrono::steady_clock>;

}  // namespace genny::v1

#endif  // HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED
