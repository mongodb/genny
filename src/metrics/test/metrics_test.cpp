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

    static void advance(v1::Period<clock_type> inc = 1ns) {
        _now += inc;
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

    auto events = v1::OperationImpl<RegistryClockSourceStub>::EventSeries{};
    auto op = v1::OperationImpl<RegistryClockSourceStub>{"Actor", "Op", events};

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

TEST_CASE("metrics output format") {
    RegistryClockSourceStub::reset();
    auto metrics = v1::RegistryT<RegistryClockSourceStub>{};
    auto reporter = genny::metrics::v1::ReporterT{metrics};

    // TODO: Consider changing this test so the operations between actor threads are reported
    // "concurrently" with respect to how op.start() and RegistryClockSourceStub::advance() are
    // called.
    //
    //           +---------------------+--------------+
    // Thread 1: |        Insert       |    Remove    |
    //           +----+                +-+        +---+
    // Thread 2:      |      Insert      | Remove |
    //                +--+           +---+--------+
    // Thread 3:         | Greetings |
    //                   +-----------+

    RegistryClockSourceStub::advance(5ns);
    {
        auto op = metrics.operation(1u, "InsertRemove", "Remove");
        auto ctx = op.start();
        ctx.addDocuments(6);
        ctx.addBytes(40);

        RegistryClockSourceStub::advance(5ns);
        ctx.success();
    }
    RegistryClockSourceStub::advance(5ns);
    {
        auto op = metrics.operation(2u, "InsertRemove", "Remove");
        auto ctx = op.start();
        ctx.addDocuments(7);
        ctx.addBytes(30);

        RegistryClockSourceStub::advance(10ns);
        ctx.success();
    }
    RegistryClockSourceStub::advance(5ns);
    {
        auto op = metrics.operation(3u, "HelloWorld", "Greetings");
        auto ctx = op.start();
        ctx.addIterations(2);

        RegistryClockSourceStub::advance(15ns);
        ctx.success();
    }
    RegistryClockSourceStub::advance(5ns);
    {
        auto op = metrics.operation(2u, "InsertRemove", "Insert");
        auto ctx = op.start();
        ctx.addDocuments(8);
        ctx.addBytes(200);

        RegistryClockSourceStub::advance(15ns);
        ctx.success();
    }
    RegistryClockSourceStub::advance(5ns);
    {
        auto op = metrics.operation(1u, "InsertRemove", "Insert");
        auto ctx = op.start();
        ctx.addDocuments(9);
        ctx.addBytes(300);

        RegistryClockSourceStub::advance(20ns);
        ctx.success();
    }
    RegistryClockSourceStub::advance(5ns);

    SECTION("csv reporting") {
        auto expected =
            "Clocks\n"
            "SystemTime,42000000\n"
            "MetricsTime,95\n"
            "\n"
            "Counters\n"
            "45,HelloWorld.id-3.Greetings_bytes,0\n"
            "90,InsertRemove.id-1.Insert_bytes,300\n"
            "65,InsertRemove.id-2.Insert_bytes,200\n"
            "25,InsertRemove.id-2.Remove_bytes,30\n"
            "10,InsertRemove.id-1.Remove_bytes,40\n"
            "45,HelloWorld.id-3.Greetings_docs,0\n"
            "90,InsertRemove.id-1.Insert_docs,9\n"
            "65,InsertRemove.id-2.Insert_docs,8\n"
            "25,InsertRemove.id-2.Remove_docs,7\n"
            "10,InsertRemove.id-1.Remove_docs,6\n"
            "45,HelloWorld.id-3.Greetings_iters,2\n"
            "90,InsertRemove.id-1.Insert_iters,1\n"
            "65,InsertRemove.id-2.Insert_iters,1\n"
            "25,InsertRemove.id-2.Remove_iters,1\n"
            "10,InsertRemove.id-1.Remove_iters,1\n"
            "\n"
            "Gauges\n"
            "\n"
            "Timers\n"
            "45,HelloWorld.id-3.Greetings_timer,15\n"
            "90,InsertRemove.id-1.Insert_timer,20\n"
            "65,InsertRemove.id-2.Insert_timer,15\n"
            "25,InsertRemove.id-2.Remove_timer,10\n"
            "10,InsertRemove.id-1.Remove_timer,5\n"
            "\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, "csv");
        REQUIRE(out.str() == expected);
    }
}

TEST_CASE("Genny.Setup metric should only be reported as a timer") {
    RegistryClockSourceStub::reset();
    auto metrics = v1::RegistryT<RegistryClockSourceStub>{};
    auto reporter = genny::metrics::v1::ReporterT{metrics};

    // Mimic what the DefaultDriver would be doing.
    auto setup = metrics.operation(0u, "Genny", "Setup");

    RegistryClockSourceStub::advance(5ns);
    auto ctx = setup.start();

    RegistryClockSourceStub::advance(10ns);
    ctx.success();

    auto expected =
        "Clocks\n"
        "SystemTime,42000000\n"
        "MetricsTime,15\n"
        "\n"
        "Counters\n"
        "\n"
        "Gauges\n"
        "\n"
        "Timers\n"
        "15,Genny.Setup,10\n"
        "\n";

    std::ostringstream out;
    reporter.report<ReporterClockSourceStub>(out, "csv");
    REQUIRE(out.str() == expected);
}

TEST_CASE("Genny.ActiveActors metric should be reported as a counter") {
    RegistryClockSourceStub::reset();
    auto metrics = v1::RegistryT<RegistryClockSourceStub>{};
    auto reporter = genny::metrics::v1::ReporterT{metrics};

    // Mimic what the DefaultDriver would be doing.
    auto startedActors = metrics.operation(0u, "Genny", "ActorStarted");
    auto finishedActors = metrics.operation(0u, "Genny", "ActorFinished");

    auto startActor = [&]() {
        auto ctx = startedActors.start();
        ctx.addDocuments(1);
        ctx.success();
    };

    auto finishActor = [&]() {
        auto ctx = finishedActors.start();
        ctx.addDocuments(1);
        ctx.success();
    };

    // Start 2 actors, have 1 finish, start 1 more, and have the remaining 2 finish.
    RegistryClockSourceStub::advance(5ns);
    startActor();
    RegistryClockSourceStub::advance(10ns);
    startActor();
    RegistryClockSourceStub::advance(20ns);
    finishActor();
    RegistryClockSourceStub::advance(50ns);
    startActor();
    RegistryClockSourceStub::advance(100ns);
    finishActor();
    RegistryClockSourceStub::advance(200ns);
    finishActor();

    auto expected =
        "Clocks\n"
        "SystemTime,42000000\n"
        "MetricsTime,385\n"
        "\n"
        "Counters\n"
        "5,Genny.ActiveActors,1\n"
        "15,Genny.ActiveActors,2\n"
        "35,Genny.ActiveActors,1\n"
        "85,Genny.ActiveActors,2\n"
        "185,Genny.ActiveActors,1\n"
        "385,Genny.ActiveActors,0\n"
        "\n"
        "Gauges\n"
        "\n"
        "Timers\n"
        "\n";

    std::ostringstream out;
    reporter.report<ReporterClockSourceStub>(out, "csv");
    REQUIRE(out.str() == expected);
}

}  // namespace
}  // namespace genny::metrics
