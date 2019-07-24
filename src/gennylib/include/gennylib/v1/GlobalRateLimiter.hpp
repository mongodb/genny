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
 * 2. The burst size should either be 1, or roughly equal to the number of
 * actors using this rate limiter. If you have a large number of threads
 * (using this rate limiter) but a small burst size and a high frequency rate,
 * you may experience bad performance.
 *
 * Inspired by
 * https://github.com/facebook/folly/blob/7c6897aa18e71964e097fc238c93b3efa98b2c61/folly/TokenBucket.h
 */
template <typename ClockT = std::chrono::steady_clock>
class BaseGlobalRateLimiter {
public:
    // This should be replaced with std::hardware_destructive_interference_size, which is
    // part of c++17 but not part of any (major) standard library. Search P0154R1
    // for more information.
    //
    // 64 is the cache line size for recent Intel and AMD processors.
    static const int CacheLineSize = 64;

    static_assert(ClockT::is_steady, "Clock must be steady");
    static_assert(std::is_same<typename ClockT::duration, std::chrono::nanoseconds>::value,
                  "Clock representation must be nano seconds");

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
    bool consumeIfWithinRate(const typename ClockT::time_point& now) {
        // This if-block deviates from the "burst" behavior of the default token-bucket
        // algorithm. Instead of having the caller burst, we parallelize the burst
        // behavior by granting one token to each consumer thread across as many threads
        // as possible, up to "_burstSize".
        //
        // This means we basically have two serial token bucket rate limiters. We first
        // check the bucket for _burstCount, and proceeed if there are tokens available
        // (i.e. canBurst is true). If not, we fallback to the token bucket for
        // _lastEmptiedTimeNS and check if the emptied time is in the future.
        if (_burstSize > 1) {
            int64_t curBurstCount = _burstCount.load();
            const bool canBurst = (curBurstCount % _burstSize) != 0;
            if (canBurst) {
                return _burstCount.compare_exchange_weak(curBurstCount, curBurstCount + 1);
            }
        }

        // The time the bucket was emptied before this consumeIfWithinRate() call.
        int64_t curEmptiedTime = _lastEmptiedTimeNS.load();

        // The time the bucket was emptied after this consumeIfWithinRate() call.
        const auto newEmptiedTime = curEmptiedTime + getRate();

        // If the new emptied time is in the future, the bucket is empty. Return early.
        if (now.time_since_epoch().count() < newEmptiedTime) {
            return false;
        }

        // Use the "weak" version for performance at the expense of false negatives (i.e.
        // `compare_exchange` not comparing equal when it should).
        const auto success = _lastEmptiedTimeNS.compare_exchange_weak(curEmptiedTime, newEmptiedTime);

        // Note that incrementing _burstCount is *not* atomic with incrementing _lastEmptiedTimeNS.
        // This may cause some threads to see an outdated _burstCount, causing unnecessary waiting
        // in the caller. For this reason, the caller should ensure the number of tokens does not
        // greatly exceed _burstSize.
        if (success) {
            _burstCount++;
        }
        return success;

    }

    constexpr int64_t getRate() const {
        return _rateNS;
    }

    /**
     * Get the number of threads using this rate limiter. This number can help the caller
     * decide how congested the rate limiter is and find an appropriate time to wait until
     * retrying.
     *
     * E.g. if there are X users, each caller on average gets called per (_rateNS * _numUsers).
     * So it makes sense for each caller to wait for a duration of the same magnitude.
     * @return
     */
    constexpr int64_t getNumUsers() const {
        return _numUsers;
    }

    void addUser() {
        _numUsers++;
    }

    /**
     * The rate limiter should be reset to allow one thread to run _burstSize number of times before
     * the start of each phase.
     */
    void resetLastEmptied() noexcept {
        _lastEmptiedTimeNS = ClockT::now().time_since_epoch().count() - _rateNS;
    }

private:
    // Manually align _lastEmptiedTimeNS and _burstCount here to vastly improve performance.
    // Lazily initialized by the first call to consumeIfWithinRate().
    // Note that std::chrono::time_point is not trivially copyable and can't be used here.
    alignas(BaseGlobalRateLimiter::CacheLineSize) std::atomic_int64_t _lastEmptiedTimeNS = 0;
    // _burstCount stores the remaining
    alignas(BaseGlobalRateLimiter::CacheLineSize) std::atomic_int64_t _burstCount = 0;

    // Note that the rate limiter as-is doesn't use the burst size, but it is cleaner to
    // store the burst size and the rate together, since they're specified together in
    // the YAML as RateSpec.
    const int64_t _burstSize;
    const int64_t _rateNS;

    // Number of threads using this rate limiter.
    int64_t _numUsers = 0;
};

using GlobalRateLimiter = BaseGlobalRateLimiter<std::chrono::steady_clock>;

}  // namespace genny::v1

#endif  // HEADER_FE10BCC4_FF45_4D79_B92F_72CE19437F81_INCLUDED
