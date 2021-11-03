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

#include <boost/log/trivial.hpp>

#include <chrono>
#include <ratio>
#include <thread>

#include <gennylib/GlobalRateLimiter.hpp>
#include <gennylib/PhaseLoop.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/clocks.hpp>
#include <testlib/helpers.hpp>

namespace genny::testing {

namespace {
using Catch::Matchers::Matches;

TEST_CASE("Dummy Clock") {
    struct DummyTemplateValue {};
    using MyDummyClock = DummyClock<DummyTemplateValue>;

    auto getTicks = [](const MyDummyClock::duration& d) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
    };

    SECTION("Can be converted to time points") {
        auto now = MyDummyClock::now().time_since_epoch();
        REQUIRE(getTicks(now) == 0);

        MyDummyClock::nowRaw++;
        now = MyDummyClock::now().time_since_epoch();
        REQUIRE(getTicks(now) == 1);
    }
}

TEST_CASE("Global rate limiter") {
    struct DummyTemplateValue {};
    using MyDummyClock = DummyClock<DummyTemplateValue>;

    const int64_t per = 3;
    const int64_t burst = 2;
    const BaseRateSpec rs{per, burst};  // 2 operations per 3 ticks.
    BaseGlobalRateLimiter<MyDummyClock> grl{rs};

    SECTION("Limits Rate") {
        grl.resetLastEmptied();
        auto now = MyDummyClock::now();

        // consumeIfWithinRate() should succeed because we allow "burst" number of ops at the
        // beginning.
        for (int i = 0; i < burst; i++) {
            REQUIRE(grl.consumeIfWithinRate(now));
        }

        // The next call should fail because we have not incremented the clock.
        REQUIRE(!grl.consumeIfWithinRate(now));

        // Increment the clock should allow consumeIfWithinRate() to succeed exactly once.
        MyDummyClock::nowRaw += per;
        now = MyDummyClock::now();
        for (int i = 0; i < burst; i++) {
            REQUIRE(grl.consumeIfWithinRate(now));
        }
        REQUIRE(!grl.consumeIfWithinRate(now));
    }
}

TEST_CASE("Percentile rate limiting") {
    struct DummyTemplateValue {};
    using MyDummyClock = DummyClock<DummyTemplateValue>;

    const int64_t percent = 50;  // 50%
    const PercentileRateSpec rs{percent};
    BaseGlobalRateLimiter<MyDummyClock> grl{rs};
    grl.addUser();

    SECTION("Limits Rate") {
        grl.resetLastEmptied();
        auto now = MyDummyClock::now();

        // consumeIfWithinRate() should succeed because we allow as many ops as desired until
        // limiting.
        for (int i = 0; i < 9; i++) {
            REQUIRE(grl.consumeIfWithinRate(now));
            grl.notifyOfIteration();
        }

        // Increment the clock by a minute.
        MyDummyClock::nowRaw += grl._nsPerMinute;
        // Tenth call sets the rate limit.
        REQUIRE(grl.consumeIfWithinRate(now));
        // Now we should only be able to call exactly half as many times.
        now = MyDummyClock::now();
        for (int i = 0; i < 5; i++) {
            REQUIRE(grl.consumeIfWithinRate(now));
        }
        REQUIRE(!grl.consumeIfWithinRate(now));

        // Then it works again a minute later.
        MyDummyClock::nowRaw += grl._nsPerMinute;
        now = MyDummyClock::now();
        for (int i = 0; i < 5; i++) {
            REQUIRE(grl.consumeIfWithinRate(now));
        }
        REQUIRE(!grl.consumeIfWithinRate(now));

        // Then starting a new phase clears the limit.
        grl.resetLastEmptied();
        now = MyDummyClock::now();
        for (int i = 0; i < 10; i++) {
            REQUIRE(grl.consumeIfWithinRate(now));
            grl.notifyOfIteration();
        }
    }
}


class IncActor : public Actor {
public:
    struct IncCounter : WorkloadContext::ShareableState<std::atomic_int64_t> {};

    IncActor(genny::ActorContext& ac, ActorId id)
        : Actor(ac, id),
          _loop{ac},
          _counter{WorkloadContext::getActorSharedState<IncActor, IncCounter>()} {
        _counter.store(0);
    };

    void run() override {
        for (auto&& config : _loop) {
            for (auto _ : config) {
                //                BOOST_LOG_TRIVIAL(info) << "Incrementing";
                ++_counter;
            }
        }
    };

    static std::string_view defaultName() {
        return "IncActor";
    }

private:
    struct PhaseConfig {
        explicit PhaseConfig(PhaseContext& context){};
    };

    IncCounter& _counter;
    PhaseLoop<PhaseConfig> _loop;
};

auto getCurState() {
    return WorkloadContext::getActorSharedState<IncActor, IncActor::IncCounter>().load();
}

auto resetState() {
    return WorkloadContext::getActorSharedState<IncActor, IncActor::IncCounter>().store(0);
}


auto incProducer = std::make_shared<DefaultActorProducer<IncActor>>("IncActor");

TEST_CASE("Global rate limiter can be used by phase loop", "[benchmark]") {
    using namespace std::chrono_literals;

    SECTION("Works with no Repeat or Duration") {
        NodeSource ns(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: IncActor
  Threads: 1
  Phases:
    - Duration: 50 milliseconds 
      GlobalRate: 7 per 20 milliseconds
    - Duration: 50 milliseconds 
      GlobalRate: 8 per 100 milliseconds

- Name: Two
  Type: IncActor
  Threads: 1
  Phases:
    - Blocking: None
      GlobalRate: 8 per 100 milliseconds

      # When the phase ends the rate limit is reset and all threads
      # are immediately woken up.
    - Blocking: None
      GlobalRate: 7 per 20 milliseconds
)",
                      "");
        auto& config = ns.root();
        int num_threads = 2;

        genny::ActorHelper ah{config, num_threads, {{"IncActor", incProducer}}};
        auto runInBg = [&ah]() { ah.run(); };
        std::thread t(runInBg);
        std::this_thread::sleep_for(110ms);
        t.join();

        const auto state = getCurState();
        REQUIRE(state == 72);
    }

    // The rate interval needs to be large enough to avoid sporadic failures, which makes
    // this test take longer. It therefore has the "[slow]" label.
    SECTION("Prevents execution when the rate is exceeded", "[slow][benchmark]") {
        NodeSource ns(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: IncActor
  Threads: 50
  Phases:
    - Duration: 520 milliseconds
      GlobalRate: 1 per 50 milliseconds
)",
                      "");
        auto& config = ns.root();
        int num_threads = 50;

        genny::ActorHelper ah{config, num_threads, {{"IncActor", incProducer}}};

        auto runInBg = [&ah]() { ah.run(); };
        std::thread t(runInBg);
        std::this_thread::sleep_for(110ms);

        // after 110ms, exactly 3 invocations should have made it through
        const auto preJoinState = getCurState();

        t.join();

        REQUIRE(getCurState() == 11);
        REQUIRE(preJoinState == 3);
    }
}

TEST_CASE("Rate Limiter Try 2", "[slow][benchmark]") {
    SECTION("Doesn't iterate too many times or sleep unnecessarily") {
        NodeSource ns(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: IncActor
  Threads: 5
  Phases:
  - GlobalRate: 1 per 50 milliseconds
    Duration: 215 milliseconds
)",
                      "");
        auto& config = ns.root();
        int num_threads = 5;

        auto fun = [&]() -> auto {
            auto start = std::chrono::steady_clock::now();
            genny::ActorHelper ah{config, num_threads, {{"IncActor", incProducer}}};
            ah.run();
            return std::chrono::steady_clock::now() - start;
        };

        resetState();
        REQUIRE(getCurState() == 0);

        auto dur = fun();

        // Shouldn't take longer than an even multiple of the rate-spec
        REQUIRE(dur.count() <= 280 * 1e6);
        // Should take at least as long as the Duration
        REQUIRE(dur.count() >= 215 * 1e6);

        const auto endState = getCurState();

        // Should have incremented 4 times in the "perfect" case
        // but 5 times if there's any timing edge-cases.
        REQUIRE(endState >= 4);
        REQUIRE(endState <= 5);
    }
}

TEST_CASE("Rate Limiter Try 3", "[slow][benchmark]") {
    SECTION("Doesn't iterate too many times or sleep unnecessarily") {
        NodeSource ns(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: IncActor
  Threads: 3
  Phases:
  - GlobalRate: 3 per 500 milliseconds
    Duration: 1200 milliseconds

)",
                      "");
        auto& config = ns.root();
        int num_threads = 3;

        auto fun = [&]() -> auto {
            auto start = std::chrono::steady_clock::now();
            genny::ActorHelper ah{config, num_threads, {{"IncActor", incProducer}}};
            ah.run();
            return std::chrono::steady_clock::now() - start;
        };

        resetState();
        REQUIRE(getCurState() == 0);

        auto dur = fun();

        const auto endState = getCurState();

        REQUIRE(endState == 9);
    }
}


}  // namespace
}  // namespace genny::testing
