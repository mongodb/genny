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

#include <metrics/metrics.hpp>

#if defined(__APPLE__)

#include <mach/mach_time.h>

#endif

genny::TimeSpec operator""_ts(unsigned long long v) {
    return genny::TimeSpec(std::chrono::milliseconds{v});
}

namespace genny::canaries {

/**
 * Use `rdtsc` as a low overhead, high resolution clock.
 *
 * Adapted from Google Benchmark:
 * x86_64/amd64: https://github.com/google/benchmark/blob/8f7b8dd9a3211e6043e742a383ccb35eb810829f/src/cycleclock.h#L82-L85
 * arm64: https://github.com/google/benchmark/blob/8f7b8dd9a3211e6043e742a383ccb35eb810829f/src/cycleclock.h#L142-L149
 */
inline Nanosecond now() {
#if defined(__APPLE__)
    return mach_absolute_time();
#elif defined(__x86_64__) || defined(__amd64__)
    uint64_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) | low;
#elif defined(__aarch64__)
    int64_t virtual_timer_value;
    asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
    return virtual_timer_value;
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

template <class Task, class... Args>
Nanosecond Loops<Task, Args...>::simpleLoop(Args&&... args) {
    auto task = Task(std::forward<Args>(args)...);

    int64_t before = now();
    for (int i = 0; i < _iterations; i++) {
        task.run();
    }
    int64_t after = now();

    return after - before;
}

template <class Task, class... Args>
Nanosecond Loops<Task, Args...>::phaseLoop(Args&&... args) {

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
    auto task = Task(std::forward<Args>(args)...);

    int64_t before = now();
    for (auto _ : loop)
        task.run();
    int64_t after = now();

    return after - before;
}

template <class Task, class... Args>
Nanosecond Loops<Task, Args...>::metricsLoop(Args&&... args) {

    auto metrics = genny::metrics::Registry{false};

    auto dummyOp = metrics.operation("metricsLoop", "dummyOp", 0u);
    auto task = Task(std::forward<Args>(args)...);

    int64_t before = now();
    for (int i = 0; i < _iterations; i++) {
        auto ctx = dummyOp.start();
        task.run();
        ctx.success();
    }
    int64_t after = now();

    return after - before;
}

template <class Task, class... Args>
Nanosecond Loops<Task, Args...>::metricsFtdcLoop(Args&&... args) {
    auto metrics = genny::metrics::Registry{false};
    auto dummyOp = metrics.operation("metricsFtdcLoop", "dummyOp", 0u);

    auto task = Task(std::forward<Args>(args)...);

    int64_t before = now();
    for (int i = 0; i < _iterations; i++) {
        auto ctx = dummyOp.start();
        task.run();
        ctx.success();
    }
    int64_t after = now();
    return after - before;
}


template <class Task, class... Args>
Nanosecond Loops<Task, Args...>::metricsPhaseLoop(Args&&... args) {

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
    auto task = Task(std::forward<Args>(args)...);

    auto metrics = genny::metrics::Registry{};

    auto dummyOp = metrics.operation("metricsLoop", "dummyOp", 0u);

    int64_t before = now();
    for (auto _ : loop) {
        auto ctx = dummyOp.start();
        task.run();
        ctx.success();
    }
    int64_t after = now();

    return after - before;
}

template <class Task, class... Args>
Nanosecond Loops<Task, Args...>::metricsFtdcPhaseLoop(Args&&... args) {

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
    auto task = Task(std::forward<Args>(args)...);

    auto metrics = genny::metrics::Registry{false};

    auto dummyOp = metrics.operation("metricsLoop", "dummyOp", 0u);

    int64_t before = now();
    for (auto _ : loop) {
        auto ctx = dummyOp.start();
        task.run();
        ctx.success();
    }
    int64_t after = now();

    return after - before;
}


// "The definition of ... a non-exported member function or static data member
// of a class template shall be present in every translation unit in which it
// is explicitly instantiated."
template class Loops<NopTask>;
template class Loops<SleepTask>;
template class Loops<CPUTask>;
template class Loops<L2Task>;
template class Loops<L3Task>;
template class Loops<PingTask, std::string&>;

}  // namespace genny::canaries
