#include "test.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <yaml-cpp/yaml.h>

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

struct PhaseConfig {
    PhaseConfig(PhaseContext& phaseContext) {}
};

struct IncrementsActor : public Actor {

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

    void run() override {
        for (int j = 0; j < 10000; ++j) {
            if (!stop) {
                ++increments;
            }
        }
    }
};


atomic_bool IncrementsRunnable::stop = false;
atomic_int IncrementsActor::increments = 0;
atomic_int IncrementsRunnable::increments = 0;

}  // namespace


TEST_CASE("PhaseLoop performance", "[perf]") {
    Orchestrator o;
    metrics::Registry registry;

    auto yaml = YAML::Load(R"(
    SchemaVersion: 2018-07-01
    Actors:
    - Type: Increments
      Threads: 500
      Phases:
      - Repeat: 10000
    )");

    WorkloadContext workloadContext{
        yaml, registry, o, "mongodb://localhost:27017", {IncrementsActor::producer()}};

    o.addRequiredTokens(500);

    std::vector<std::thread> actors;
    boost::barrier actorBarrier(1);
    for (auto& actor : workloadContext.actors()) {
        actors.emplace_back([&]() {
            actorBarrier.wait();
            actor->run();
        });
    }
    auto actorStart = std::chrono::steady_clock::now();
    actorBarrier.count_down_and_wait();
    for (auto& actor : actors)
        actor.join();
    auto actorDur = duration_cast<nanoseconds>(steady_clock::now() - actorStart).count();
    std::cout << "Took " << actorDur << " nanoseconds for PhaseLoop loop" << std::endl;

    REQUIRE(IncrementsActor::increments == 500 * 10000);

    boost::barrier regBarrier(1);
    std::vector<std::unique_ptr<IncrementsRunnable>> runners;
    for (int i = 0; i < 500; ++i) {
        runners.emplace_back(std::make_unique<IncrementsRunnable>());
    }
    std::vector<std::thread> regulars;
    for (auto& runnable : runners) {
        regulars.emplace_back([&]() {
            regBarrier.wait();
            runnable->run();
        });
    }

    // keep the compiler from being clever about
    // removing check to `if(!stop)`
    auto stopper = std::thread([&]() {
        std::this_thread::sleep_for(seconds{2});
        IncrementsRunnable::stop = true;
    });

    auto regStart = std::chrono::steady_clock::now();
    regBarrier.count_down_and_wait();
    for (auto& reg : regulars)
        reg.join();
    auto regDur = duration_cast<nanoseconds>(steady_clock::now() - regStart).count();
    std::cout << "Took " << regDur << " nanoseconds for regular for loop" << std::endl;
    REQUIRE(IncrementsRunnable::increments == 500 * 10000);

    stopper.join();

    // we're no less than 100 times worse
    // INFO(double(regDur) / double(actorDur));
    REQUIRE(actorDur <= regDur * 100);
}
