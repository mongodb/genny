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

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <boost/format.hpp>
#include <boost/thread/barrier.hpp>

#include <gennylib/Orchestrator.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

using namespace genny;
using namespace genny::v1;
using namespace std;
using namespace std::chrono;

namespace {

struct IncrementsActor : public Actor {

    struct PhaseConfig {
        PhaseConfig(PhaseContext& phaseContext) {}
    };

    static atomic_int increments;

    PhaseLoop<PhaseConfig> _loop;

    IncrementsActor(ActorContext& ctx) : Actor(ctx), _loop{ctx} {}

    void run() override {
        for (auto&& config : _loop) {
            for (auto&& _ : config) {
                ++increments;
            }
        }
    }
};

struct VirtualRunnable {
    virtual void run() = 0;
};

struct IncrementsRunnable : public VirtualRunnable {
    static atomic_bool stop;
    static atomic_int increments;

    const long iterations;

    explicit IncrementsRunnable(long iters) : iterations{iters} {}

    // virtual method just like Actor::run()
    void run() override {
        for (int j = 0; j < iterations; ++j) {
            // check an atomic_bool at each iteration just like
            // we do in Orchestrator+PhaseLoop. Don't want that
            // impact to be considered.
            if (!stop) {
                ++increments;
            }
        }
    }
};


atomic_bool IncrementsRunnable::stop = false;
atomic_int IncrementsActor::increments = 0;
atomic_int IncrementsRunnable::increments = 0;

using clock = std::chrono::steady_clock;

template <typename Runnables>
int64_t timedRun(Runnables&& runnables) {
    std::vector<std::thread> threads;
    boost::barrier startWait(runnables.size() + 1);
    boost::barrier endWait(runnables.size() + 1);
    for (auto& runnable : runnables) {
        threads.emplace_back([&]() {
            startWait.wait();
            runnable->run();
            endWait.wait();
        });
    }

    auto start = clock::now();
    startWait.count_down_and_wait();
    endWait.count_down_and_wait();
    auto duration = duration_cast<nanoseconds>(clock::now() - start).count();

    for (auto& thread : threads)
        thread.join();

    return duration;
}


auto runRegularThreads(int threads, long iterations) {
    IncrementsRunnable::increments = 0;
    std::vector<std::unique_ptr<IncrementsRunnable>> runners;
    for (int i = 0; i < threads; ++i)
        runners.emplace_back(std::make_unique<IncrementsRunnable>(iterations));
    auto regDur = timedRun(runners);
    REQUIRE(IncrementsRunnable::increments == threads * iterations);
    return regDur;
}

auto runActors(int threads, long iterations) {
    IncrementsActor::increments = 0;
    auto configString = boost::format(R"(
    SchemaVersion: 2018-07-01
    Actors:
    - Type: Increments
      Name: Increments
      Threads: %i
      Phases:
      - Repeat: %i
    )") %
        threads % iterations;
    auto config = NodeSource(configString.str(), "");

    auto incProducer = std::make_shared<DefaultActorProducer<IncrementsActor>>("Increments");

    int64_t actorDur;

    ActorHelper ac(config.root(), threads, {{"Increments", incProducer}});
    ac.run([&actorDur](const WorkloadContext& wc) { actorDur = timedRun(wc.actors()); });

    REQUIRE(IncrementsActor::increments == threads * iterations);
    return actorDur;
}

void comparePerformance(int threads, long iterations, int tolerance) {
    // just do the stupid simple thing and run it 5 times and take the mean, no need to make it
    // fancy...

    auto reg1 = runRegularThreads(threads, iterations);
    auto act1 = runActors(threads, iterations);

    // but run in different orders so the CPU caches don't affect too much

    auto act2 = runActors(threads, iterations);
    auto reg2 = runRegularThreads(threads, iterations);

    auto act3 = runActors(threads, iterations);
    auto reg3 = runRegularThreads(threads, iterations);

    auto act4 = runActors(threads, iterations);
    auto act5 = runActors(threads, iterations);

    auto reg4 = runRegularThreads(threads, iterations);
    auto reg5 = runRegularThreads(threads, iterations);

    auto regMean = double(reg1 + reg2 + reg3 + reg4 + reg5) / 5.;
    auto actMean = double(act1 + act2 + act3 + act4 + act5) / 5.;

    // we're no less than tolerance times worse
    INFO("threads=" << threads << ",iterations=" << iterations << ", actor mean " << actMean
                    << " <= regular mean " << regMean << " * " << tolerance << "("
                    << (regMean * tolerance) << "). Ratio = " << double(actMean) / double(regMean));

    REQUIRE(actMean <= regMean * tolerance);
}

}  // namespace


TEST_CASE("PhaseLoop performance", "[benchmark]") {
    // low tolerance for added latency with few threads
    comparePerformance(50, 10000, 5);
    comparePerformance(10, 100000, 10);
    // higher tolerance for added latency with more threads
    comparePerformance(500, 10000, 100);
}
