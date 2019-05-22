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

#include <gennylib/PhaseLoop.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

namespace genny::testing {

namespace {

TEST_CASE("Find max performance of rate limiter", "[benchmark]") {

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

    NodeSource config(R"(
SchemaVersion: 2018-07-01
Actors:
- Name: One
  Type: IncActor
  Threads: 50
  Phases:
    - Duration: 10 seconds
      GlobalRate: 1 per 100 microseconds
    )",
                      "");

    auto incProducer = std::make_shared<DefaultActorProducer<IncActor>>("IncActor");
    int num_threads = 50;

    genny::ActorHelper ah{config.root(), num_threads, {{"IncActor", incProducer}}};
    auto getCurState = []() {
        return WorkloadContext::getActorSharedState<IncActor, IncActor::IncCounter>().load();
    };

    ah.run();

    // 10 * 1000 ops per second * 10 seconds.
    int64_t expected = 10 * 1000 * 10;

    // Result is at least 95% of the expected value. There's some uncertainty
    // due to manually induced jitter.
    REQUIRE(getCurState() > expected * 0.90);

    // Result is at most 110% of the expected value. The steady clock
    // time is cached for threads so threads can run for longer than the
    // specified duration.
    REQUIRE(getCurState() < expected * 1.10);

    // Print out the result if both REQUIRE pass.
    BOOST_LOG_TRIVIAL(info) << getCurState();
}
}  // namespace
}  // namespace genny::testing
