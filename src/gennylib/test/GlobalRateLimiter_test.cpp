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

    SECTION("Stores the burst size and rate") {
        REQUIRE(grl.getBurstSize() == burst);
        REQUIRE(grl.getRate() == per);
    }

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
                BOOST_LOG_TRIVIAL(info) << "Incrementing";
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

auto getCurState = []() {
    return WorkloadContext::getActorSharedState<IncActor, IncActor::IncCounter>().load();
};

auto resetState = []() {
    return WorkloadContext::getActorSharedState<IncActor, IncActor::IncCounter>().store(0);
};


auto incProducer = std::make_shared<DefaultActorProducer<IncActor>>("IncActor");

TEST_CASE("Global rate limiter can be used by phase loop") {
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
}
}  // namespace
}  // namespace genny::testing
