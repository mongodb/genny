#include <algorithm>
#include <cassert>
#include <thread>
#include <vector>

#include <gennylib/MetricsReporter.hpp>
#include <gennylib/PhasedActor.hpp>
#include <gennylib/actors/HelloWorld.hpp>

#include "DefaultDriver.hpp"

int genny::driver::DefaultDriver::run(int, char **) const {
  const int nActors = 2;

  genny::metrics::Registry metrics;
  auto orchestrator = Orchestrator{nActors};

  auto actorSetupTimer = metrics.timer("actorSetup");
  auto stopwatch = actorSetupTimer.start();
  std::vector<std::unique_ptr<genny::PhasedActor>> actors;
  actors.push_back(
      std::make_unique<genny::actor::HelloWorld>(orchestrator, metrics, "one"));
  actors.push_back(
      std::make_unique<genny::actor::HelloWorld>(orchestrator, metrics, "two"));
  stopwatch.report();

  assert(actors.size() == nActors);

  std::mutex lock;
  auto threadCounter = metrics.counter("threadCounter");
  std::vector<std::thread> threads;
  std::transform(cbegin(actors), cend(actors), std::back_inserter(threads),
                 [&](const auto &actor) {
                   return std::thread{[&]() {
                     lock.lock();
                     threadCounter.incr();
                     lock.unlock();
                     actor->run();
                     lock.lock();
                     threadCounter.decr();
                     lock.unlock();
                   }};
                 });

  for (auto &thread : threads)
    thread.join();

  const auto reporter = genny::metrics::Reporter{metrics};
  reporter.report(std::cout);

  return 0;
}
