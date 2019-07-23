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

#include <gennylib/PhaseLoop.hpp>
#include <gennylib/v1/GlobalRateLimiter.hpp>

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
    const RateSpec rs{per, burst};  // 2 operations per 3 ticks.
    v1::BaseGlobalRateLimiter<MyDummyClock> grl{rs};

    SECTION("Limits Rate") {
        auto now = MyDummyClock::now();

        // consumeIfWithinRate() should fail because we have not incremented the clock.
        REQUIRE(!grl.consumeIfWithinRate(now));

        // Increment the clock should allow consumeIfWithinRate() to succeed exactly once.
        MyDummyClock::nowRaw += per;
        now = MyDummyClock::now();
        REQUIRE(grl.consumeIfWithinRate(now));
        REQUIRE(!grl.consumeIfWithinRate(now));
    }
}


class IncActor : public Actor {
public:
    struct IncCounter : WorkloadContext::ShareableState<std::atomic_int64_t> {};

    IncActor(genny::ActorContext& ac)
        : Actor(ac),
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

    SECTION("Fail if no Repeat or Duration") {
        NodeSource ns(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: IncActor
  Threads: 1
  Phases:
    - GlobalRate: 5 per 4 seconds
      Blocking: None
)",
                      "");
        auto& config = ns.root();
        int num_threads = 2;

        auto fun = [&]() {
            genny::ActorHelper ah{config, num_threads, {{"IncActor", incProducer}}};
        };
        REQUIRE_THROWS_WITH(fun(), Matches(R"(.*alongside either Duration or Repeat.*)"));
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
