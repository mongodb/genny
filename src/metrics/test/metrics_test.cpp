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
#include <optional>
#include <sstream>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

namespace genny::metrics {
namespace {

using namespace std::literals::chrono_literals;

class RegistryClockSourceStub {
private:
    using clock_type = std::chrono::steady_clock;

public:
    using duration = clock_type::duration;
    using time_point = std::chrono::time_point<clock_type>;

    static void advance(v1::Period<clock_type> duration = 1ns) {
        _now += duration;
    }

    static void reset() {
        _now = {};
    }

    static time_point now() {
        return _now;
    }

private:
    static time_point _now;
};

RegistryClockSourceStub::time_point RegistryClockSourceStub::_now;

struct ReporterClockSourceStub {
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    static time_point now() {
        return time_point{42ms};
    }
};

void assertDurationsEqual(RegistryClockSourceStub::duration dur1,
                          RegistryClockSourceStub::duration dur2) {
    // We compare std:chrono::time_points by converting through Period in order to take advantage of
    // its operator<<(std::ostream&) in case the check fails.
    REQUIRE(v1::Period<RegistryClockSourceStub>{dur1} == v1::Period<RegistryClockSourceStub>{dur2});
}

TEST_CASE("metrics::OperationContext interface") {
    RegistryClockSourceStub::reset();

    auto desc = v1::OperationDescriptor{0, "Actor", "Op"};
    auto events = v1::OperationImpl<RegistryClockSourceStub>::EventSeries{};
    auto op = v1::OperationImpl<RegistryClockSourceStub>{std::move(desc), events};

    RegistryClockSourceStub::advance(5ns);
    auto ctx = std::make_optional<v1::OperationContextT<RegistryClockSourceStub>>(op);

    ctx->addDocuments(200);
    ctx->addBytes(3000);
    ctx->addErrors(4);
    RegistryClockSourceStub::advance(67ns);

    REQUIRE(events.size() == 0);

    auto expected = v1::OperationEvent<RegistryClockSourceStub>{};
    expected.iters = 1;
    expected.ops = 200;
    expected.size = 3000;
    expected.errors = 4;

    SECTION("success() reports the operation") {
        expected.duration = 67ns;
        ctx->success();
        REQUIRE(events.size() == 1);

        ctx.reset();

        expected.outcome = v1::OperationEvent<RegistryClockSourceStub>::OutcomeType::kSuccess;
        assertDurationsEqual(events[0].first.time_since_epoch(), 72ns);
        REQUIRE(events[0].second == expected);
    }

    SECTION("failure() reports the operation") {
        expected.duration = 67ns;
        ctx->failure();
        REQUIRE(events.size() == 1);

        ctx.reset();

        expected.outcome = v1::OperationEvent<RegistryClockSourceStub>::OutcomeType::kFailure;
        assertDurationsEqual(events[0].first.time_since_epoch(), 72ns);
        REQUIRE(events[0].second == expected);
    }

    SECTION("discard() doesn't report the operation") {
        ctx.reset();
        REQUIRE(events.size() == 0);
    }

    SECTION("add*() methods can be called multiple times") {
        ctx->addIterations(8);
        ctx->addIterations(9);
        ctx->addDocuments(200);
        ctx->addBytes(3000);
        ctx->addErrors(4);
        RegistryClockSourceStub::advance(67ns);

        REQUIRE(events.size() == 0);

        expected.iters = 17;
        expected.ops += 200;
        expected.size += 3000;
        expected.errors += 4;

        expected.duration = 134ns;
        ctx->success();
        REQUIRE(events.size() == 1);

        ctx.reset();

        expected.outcome = v1::OperationEvent<RegistryClockSourceStub>::OutcomeType::kSuccess;
        assertDurationsEqual(events[0].first.time_since_epoch(), 139ns);
        REQUIRE(events[0].second == expected);
    }
}

TEST_CASE("example metrics usage") {
    RegistryClockSourceStub::reset();
    v1::RegistryT<RegistryClockSourceStub> metrics;

    // pretend this is an actor's implementation

    // actor constructor
    auto queryTime = metrics.timer("client.query");
    auto phaseTime = metrics.timer("actor.phase");
    auto operations = metrics.counter("actor.operations");
    auto failures = metrics.counter("actor.failures");
    auto sessions = metrics.gauge("actor.sessions");

    // in each phase, do something things
    for (int phase = 0; phase < 10; ++phase) {
        RegistryClockSourceStub::advance();
        auto thisIter = phaseTime.raii();

        try {
            RegistryClockSourceStub::advance();
            auto q = queryTime.raii();
            sessions.set(1);

            // do something with the driver
            if (phase % 3 == 0) {
                RegistryClockSourceStub::advance();
                throw std::exception{};
            }

            operations.incr();
        } catch (...) {
            failures.incr();
        }
    }

    // would be done by framework / outside code:
    auto reporter = genny::metrics::v1::ReporterT(metrics);

    REQUIRE(reporter.getGaugeCount() == 1);
    REQUIRE(reporter.getTimerCount() == 2);
    REQUIRE(reporter.getCounterCount() == 2);

    REQUIRE(reporter.getGaugePointsCount() == 10);
    REQUIRE(reporter.getTimerPointsCount() == 20);
    REQUIRE(reporter.getCounterPointsCount() == 10);

    auto expected =
        "Clocks\n"
        "SystemTime,42000000\n"
        "MetricsTime,24\n"
        "\n"
        "Counters\n"
        "3,actor.failures,1\n"
        "10,actor.failures,2\n"
        "17,actor.failures,3\n"
        "24,actor.failures,4\n"
        "5,actor.operations,1\n"
        "7,actor.operations,2\n"
        "12,actor.operations,3\n"
        "14,actor.operations,4\n"
        "19,actor.operations,5\n"
        "21,actor.operations,6\n"
        "\n"
        "Gauges\n"
        "2,actor.sessions,1\n"
        "5,actor.sessions,1\n"
        "7,actor.sessions,1\n"
        "9,actor.sessions,1\n"
        "12,actor.sessions,1\n"
        "14,actor.sessions,1\n"
        "16,actor.sessions,1\n"
        "19,actor.sessions,1\n"
        "21,actor.sessions,1\n"
        "23,actor.sessions,1\n"
        "\n"
        "Timers\n"
        "3,actor.phase,2\n"
        "5,actor.phase,1\n"
        "7,actor.phase,1\n"
        "10,actor.phase,2\n"
        "12,actor.phase,1\n"
        "14,actor.phase,1\n"
        "17,actor.phase,2\n"
        "19,actor.phase,1\n"
        "21,actor.phase,1\n"
        "24,actor.phase,2\n"
        "3,client.query,1\n"
        "5,client.query,0\n"
        "7,client.query,0\n"
        "10,client.query,1\n"
        "12,client.query,0\n"
        "14,client.query,0\n"
        "17,client.query,1\n"
        "19,client.query,0\n"
        "21,client.query,0\n"
        "24,client.query,1\n"
        "\n";

    std::ostringstream out;
    reporter.report<ReporterClockSourceStub>(out, "csv");
    REQUIRE(out.str() == expected);
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

TEST_CASE("metrics operation test") {
    RegistryClockSourceStub::reset();
    v1::RegistryT<RegistryClockSourceStub> metrics;

    auto op = metrics.operation(0u, "InsertRemove", "Insert");

    RegistryClockSourceStub::advance();
    {
        auto ctx = op.start();
        ctx.addDocuments(6);
        ctx.addBytes(40);

        RegistryClockSourceStub::advance();
        ctx.success();
    }
    RegistryClockSourceStub::advance();

    // would be done by framework / outside code:
    auto reporter = genny::metrics::v1::ReporterT(metrics);

    SECTION("csv reporting") {
        // TODO: The thread number should be included in the name of the metrics.
        auto expected =
            "Clocks\n"
            "SystemTime,42000000\n"
            "MetricsTime,3\n"
            "\n"
            "Counters\n"
            "2,Insert_bytes,40\n"
            "2,Insert_docs,6\n"
            "2,Insert_iters,1\n"
            "\n"
            "Gauges\n"
            "\n"
            "Timers\n"
            "2,Insert_timer,1\n"
            "\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, "csv");
        REQUIRE(out.str() == expected);
    }

    SECTION("cedar-csv reporting") {
        auto expected =
            "Clocks\n"
            "MetricsTime,3\n"
            "SystemTime,42000000\n"
            "\n"
            "OperationThreadCounts\n"
            "InsertRemove,Insert,1"
            "\n"
            "Operations\n"
            "timestamp,actor,thread,operation,duration,outcome,n,ops,errors,size"
            "2,InsertRemove,0,Insert,1,0,1,6,2,40\n"
            "\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, "cedar-csv");
        REQUIRE(out.str() == expected);
    }
}

}  // namespace
}  // namespace genny::metrics
