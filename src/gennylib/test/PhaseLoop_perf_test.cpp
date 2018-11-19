#include "test.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
#include <vector>
#include <thread>
#include <mutex>

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

static atomic_int increments = 0;


namespace {

struct PhaseConfig {
    PhaseConfig(PhaseContext& phaseContext) {}
};

struct Increments : public Actor {

    PhaseLoop<PhaseConfig> _loop;

    Increments(ActorContext& ctx) : _loop{ctx} {}

    void run() override {
        long long iter = 0;
        for (auto&& [phase, config] : _loop) {
            for (auto&& _ : config) {
//                ++iter;
//                std::cout << iter << std::endl;
                ++increments;
            }
        }
    }

    static ActorProducer producer() {
        return [](ActorContext& context) {
            ActorVector out;
            for(int i=0; i < context.get<int>("Threads"); ++i) {
                out.push_back(std::make_unique<Increments>(context));
            }
            return out;
        };
    }
};

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

    WorkloadContext workloadContext {
        yaml, registry, o, "mongodb://localhost:27017", {Increments::producer()}
    };

    o.addRequiredTokens(500);

    std::vector<std::thread> actors;
    boost::barrier actorBarrier(1);
    for(auto& actor : workloadContext.actors()) {
        actors.emplace_back([&](){
            actorBarrier.wait();
            actor->run();
        });
    }
    auto actorStart = std::chrono::steady_clock::now();
    actorBarrier.count_down_and_wait();
    for(auto& actor : actors)
        actor.join();
    auto actorDur = duration_cast<milliseconds>(steady_clock::now() - actorStart).count();
    std::cout << "Took " << actorDur << " milliseconds for PhaseLoop loop" << std::endl;


    std::shared_mutex mutex;

    auto fn = [&]() {
        for(int j=0; j<10000; ++j) {
            std::shared_lock<std::shared_mutex> reader{mutex};
            ++increments;
        }
    };

    boost::barrier regBarrier(1);
    std::vector<std::thread> regulars;
    for(int i=0; i < 500; ++i) {
        regulars.emplace_back([&](){
            regBarrier.wait();
            fn();
        });
    }

    auto regStart = std::chrono::steady_clock::now();
    regBarrier.count_down_and_wait();
    for(auto& reg : regulars)
        reg.join();
    auto regDur = duration_cast<milliseconds>(steady_clock::now() - regStart).count();
    std::cout << "Took " << regDur << " milliseconds for regular for loop" << std::endl;

}