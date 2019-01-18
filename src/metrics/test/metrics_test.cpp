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

#include <iomanip>
#include <iostream>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

TEST_CASE("example metrics usage") {
    genny::metrics::Registry metrics;

    // pretend this is an actor's implementation

    // actor constructor
    auto queryTime = metrics.timer("client.query");
    auto phaseTime = metrics.timer("actor.phase");
    auto operations = metrics.counter("actor.operations");
    auto failures = metrics.counter("actor.failures");
    auto sessions = metrics.gauge("actor.sessions");

    // in each phase, do something things
    for (int phase = 0; phase < 10; ++phase) {
        auto thisIter = phaseTime.raii();

        try {
            auto q = queryTime.raii();
            sessions.set(1);

            // do something with the driver
            if (phase % 3 == 0) {
                throw std::exception{};
            }

            operations.incr();
        } catch (...) {
            failures.incr();
        }
    }

    // would be done by framework / outside code:
    auto reporter = genny::metrics::Reporter(metrics);
    reporter.report(std::cout, "csv");

    REQUIRE(reporter.getGaugeCount() == 1);
    REQUIRE(reporter.getTimerCount() == 2);
    REQUIRE(reporter.getCounterCount() == 2);

    REQUIRE(reporter.getGaugePointsCount() == 10);
    REQUIRE(reporter.getTimerPointsCount() == 20);
    REQUIRE(reporter.getCounterPointsCount() == 10);
}

TEST_CASE("metrics reporter") {
    genny::metrics::Registry reg;
    auto reporter = genny::metrics::Reporter(reg);

    SECTION("no interactions") {
        REQUIRE(reporter.getGaugeCount() == 0);
        REQUIRE(reporter.getTimerCount() == 0);
        REQUIRE(reporter.getCounterCount() == 0);

        REQUIRE(reporter.getGaugePointsCount() == 0);
        REQUIRE(reporter.getTimerPointsCount() == 0);
        REQUIRE(reporter.getCounterPointsCount() == 0);
        reporter.report(std::cout, "csv");
    }


    SECTION("registered some tokens") {
        auto g = reg.gauge("gauge");

        REQUIRE(reporter.getGaugeCount() == 1);
        REQUIRE(reporter.getTimerCount() == 0);
        REQUIRE(reporter.getCounterCount() == 0);

        REQUIRE(reporter.getGaugePointsCount() == 0);
        REQUIRE(reporter.getTimerPointsCount() == 0);
        REQUIRE(reporter.getCounterPointsCount() == 0);

        auto t = reg.timer("timer");
        auto c = reg.counter("counter");

        REQUIRE(reporter.getGaugeCount() == 1);
        REQUIRE(reporter.getTimerCount() == 1);
        REQUIRE(reporter.getCounterCount() == 1);

        REQUIRE(reporter.getGaugePointsCount() == 0);
        REQUIRE(reporter.getTimerPointsCount() == 0);
        REQUIRE(reporter.getCounterPointsCount() == 0);


        SECTION("Registering again doesn't affect count") {
            auto t2 = reg.timer("timer");
            REQUIRE(reporter.getTimerCount() == 1);


            auto anotherT = reg.timer("some.other.timer");
            REQUIRE(reporter.getTimerCount() == 2);
        }

        SECTION("Record some gauge values") {
            g.set(10);
            REQUIRE(reporter.getGaugePointsCount() == 1);
            g.set(10);
            REQUIRE(reporter.getGaugePointsCount() == 2);

            REQUIRE(reporter.getTimerPointsCount() == 0);
            REQUIRE(reporter.getCounterPointsCount() == 0);
        }

        SECTION("Record some counter values") {
            c.incr();
            REQUIRE(reporter.getCounterPointsCount() == 1);
            c.incr();
            REQUIRE(reporter.getCounterPointsCount() == 2);

            REQUIRE(reporter.getGaugePointsCount() == 0);
            REQUIRE(reporter.getTimerPointsCount() == 0);
        }

        SECTION("Record some manual-reporting timer values") {
            // no data points until .report();
            auto started = t.start();
            REQUIRE(reporter.getTimerPointsCount() == 0);
            auto started2 = t.start();
            REQUIRE(reporter.getTimerPointsCount() == 0);

            started.report();
            REQUIRE(reporter.getTimerPointsCount() == 1);

            started2.report();
            REQUIRE(reporter.getTimerPointsCount() == 2);

            // can report multiple times
            started2.report();
            started2.report();
            REQUIRE(reporter.getTimerPointsCount() == 4);

            REQUIRE(reporter.getCounterPointsCount() == 0);
            REQUIRE(reporter.getGaugePointsCount() == 0);
        }

        SECTION("Record some RAII self-reporting timer values") {
            // nothing until we hit report or close in dtor
            {
                auto r = t.raii();
                REQUIRE(reporter.getTimerPointsCount() == 0);
                auto r2 = t.raii();
                REQUIRE(reporter.getTimerPointsCount() == 0);

                // don't explicitly close this one
                auto r3 = t.raii();

                r.report();
                REQUIRE(reporter.getTimerPointsCount() == 1);

                r2.report();
                REQUIRE(reporter.getTimerPointsCount() == 2);
            }
            // 2 from when we .report()ed and 3 more from dtoring r, r2, and r3
            REQUIRE(reporter.getTimerPointsCount() == 5);

            {
                auto a1 = t.raii();
                auto a2 = t.raii();
            }
            REQUIRE(reporter.getTimerPointsCount() == 7);

            {
                auto a1 = t.raii();
                auto a2 = t.raii();
                // moving doesn't count toward closing
                auto a3 = std::move(a2);
                auto a4{std::move(a1)};
            }
            REQUIRE(reporter.getTimerPointsCount() == 9);

            REQUIRE(reporter.getCounterPointsCount() == 0);
            REQUIRE(reporter.getGaugePointsCount() == 0);
        }
    }
}

TEST_CASE("metrics tests") {
    genny::metrics::Registry reg;
    auto w = reg.timer("this_test");
    auto r = reg.timer("allocations");

    auto startReg = r.start();
    auto c = reg.counter("foo");
    auto t = reg.timer("some_operation");
    auto g = reg.gauge("sessions");
    startReg.report();

    for (int i = 0; i < 10; ++i) {
        auto wholetest{w.raii()};
        auto f = t.start();

        c.incr();
        c.incr(100);
        c.incr(-1);
        c.incr(-1);

        f.report();

        {
            auto x{t.raii()};  // automatically closed
            x.report();
            g.set(30);
        }
        { auto x{t.raii()}; }
        g.set(100);
    }

    auto reporter = genny::metrics::Reporter(reg);
    REQUIRE(reporter.getGaugeCount() == 1);
    REQUIRE(reporter.getTimerCount() == 3);
    REQUIRE(reporter.getCounterCount() == 1);

    REQUIRE(reporter.getGaugePointsCount() == 20);
    REQUIRE(reporter.getTimerPointsCount() == 51);
    REQUIRE(reporter.getCounterPointsCount() == 40);

    reporter.report(std::cout, "csv");
}