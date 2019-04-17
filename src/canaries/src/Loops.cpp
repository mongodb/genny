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

#include <canaries/Loops.hpp>

#include <chrono>
#include <optional>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>

#if defined(__APPLE__)

#include <mach/mach_time.h>

#endif

/**
 * Do something in each iteration of the loop.
 *
 * @tparam WithPing Template to dictate what to do. Right now there are only
 *                  two options, but this will be expanded over time to
 *                  support more action types.
 */
template <bool WithPing>
inline void doPingIfNeeded() {
    if constexpr (WithPing) {
        // TODO: call db.ping();
    } else {
        int x = 0;
        // Ensure memory is flushed and instruct the compiler
        // to not optimize this line out.
        asm volatile("" : : "r,m"(x++) : "memory");
    }
}

genny::TimeSpec operator""_ts(unsigned long long v) {
    return genny::TimeSpec(std::chrono::milliseconds{v});
}

namespace genny::canaries {

/**
 * Use `rdtsc` as a low overhead, high resolution clock.
 *
 * Adapted from Google Benchmark
 * https://github.com/google/benchmark/blob/439d6b1c2a6da5cb6adc4c4dfc555af235722396/src/cycleclock.h#L61
 */
inline Nanosecond now() {
#if defined(__APPLE__)
    return mach_absolute_time();
#elif defined(__x86_64__) || defined(__amd64__)
    uint64_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) | low;
#endif
}

template <bool WithPing>
Nanosecond Loops<WithPing>::simpleLoop() {
    int64_t before = now();
    for (int i = 0; i < _iterations; i++) {
        doPingIfNeeded<WithPing>();
    }
    int64_t after = now();

    return after - before;
}

template <bool WithPing>
Nanosecond Loops<WithPing>::phaseLoop() {

    Orchestrator o{};
    v1::ActorPhase<int> loop{
        o,
        std::make_unique<v1::IterationChecker>(std::nullopt,
                                               std::make_optional(IntegerSpec(_iterations)),
                                               false,
                                               0_ts,
                                               0_ts,
                                               std::nullopt),
        1};

    int64_t before = now();
    for (auto _ : loop)
        doPingIfNeeded<WithPing>();
    int64_t after = now();

    return after - before;
}

template <bool WithPing>
Nanosecond Loops<WithPing>::metricsLoop() {

    auto metrics = genny::metrics::Registry{};
    metrics::Reporter reporter(metrics);

    auto dummyOp = metrics.operation("metricsLoop", WithPing ? "db.ping()" : "Nop", 0u);

    int64_t before = now();
    for (int i = 0; i < _iterations; i++) {
        auto ctx = dummyOp.start();
        doPingIfNeeded<WithPing>();
        ctx.success();
    }
    int64_t after = now();

    return after - before;
}

template <bool WithPing>
Nanosecond Loops<WithPing>::metricsPhaseLoop() {

    // Copy/pasted from phaseLoop() and metricsLoop()
    Orchestrator o{};
    v1::ActorPhase<int> loop{
        o,
        std::make_unique<v1::IterationChecker>(std::nullopt,
                                               std::make_optional(IntegerSpec(_iterations)),
                                               false,
                                               0_ts,
                                               0_ts,
                                               std::nullopt),
        1};

    auto metrics = genny::metrics::Registry{};
    metrics::Reporter{metrics};

    auto dummyOp = metrics.operation("metricsLoop", WithPing ? "db.ping()" : "Nop", 0u);

    int64_t before = now();
    for (auto _ : loop) {
        auto ctx = dummyOp.start();
        doPingIfNeeded<WithPing>();
        ctx.success();
    }
    int64_t after = now();

    return after - before;
}

// "The definition of ... a non-exported member function or static data member
// of a class template shall be present in every translation unit in which it
// is explicitly instantiated."
template class Loops<true>;

template class Loops<false>;
}  // namespace genny::canaries
