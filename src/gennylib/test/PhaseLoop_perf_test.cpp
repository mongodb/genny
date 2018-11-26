#include "test.h"

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
#include <log.hh>

using namespace genny;
using namespace genny::V1;
using namespace std;
using namespace std::chrono;

namespace {

struct IncrementsActor : public Actor {

    struct PhaseConfig {
        PhaseConfig(PhaseContext& phaseContext) {}
    };

    static atomic_int increments;

    PhaseLoop<PhaseConfig> _loop;

    IncrementsActor(ActorContext& ctx) : _loop{ctx} {}

    void run() override {
        for (auto&& [phase, config] : _loop) {
            for (auto&& _ : config) {
                ++increments;
            }
        }
    }

    static ActorProducer producer() {
        return [](ActorContext& context) {
            ActorVector out;
            for (int i = 0; i < context.get<int>("Threads"); ++i) {
                out.push_back(std::make_unique<IncrementsActor>(context));
            }
            return out;
        };
    }
};

struct VirtualRunnable {
    virtual void run() = 0;
};

struct IncrementsRunnable : public VirtualRunnable {
    static atomic_bool stop;
    static atomic_int increments;

    const long iterations;

    explicit IncrementsRunnable(long iters)
    : iterations{iters} {}

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


template <typename Runnables>
auto timedRun(Runnables&& runnables) {
    std::vector<std::thread> threads;
    boost::barrier barrier(1);
    for (auto& runnable : runnables) {
        threads.emplace_back([&]() {
            barrier.wait();
            runnable->run();
        });
    }
    auto start = std::chrono::steady_clock::now();
    barrier.count_down_and_wait();
    for (auto& thread : threads)
        thread.join();
    auto duration = duration_cast<nanoseconds>(steady_clock::now() - start).count();
    return duration;
}


auto runRegularThreads(int threads, long iterations) {
    std::vector<std::unique_ptr<IncrementsRunnable>> runners;
    for (int i = 0; i < 500; ++i)
        runners.emplace_back(std::make_unique<IncrementsRunnable>(iterations));
    auto regDur = timedRun(runners);
    REQUIRE(IncrementsRunnable::increments == threads * iterations);
    return regDur;
}

}  // namespace


TEST_CASE("PhaseLoop performance", "[perf]") {
    Orchestrator o;
    metrics::Registry registry;
    auto yamlString = boost::format(R"(
    SchemaVersion: 2018-07-01
    Actors:
    - Type: Increments
      Threads: %i
      Phases:
      - Repeat: %i
    )") % 500 % 10000;
    auto yaml = YAML::Load(yamlString.str());
    WorkloadContext workloadContext{
        yaml, registry, o, "mongodb://localhost:27017", {IncrementsActor::producer()}};
    o.addRequiredTokens(500);
    auto actorDur = timedRun(workloadContext.actors());
    REQUIRE(IncrementsActor::increments == 500 * 10000);

    auto regDur = runRegularThreads(500, 10000);

    // we're no less than 100 times worse
    // INFO(double(regDur) / double(actorDur));
    REQUIRE(actorDur <= regDur * 100);
}
