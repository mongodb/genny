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

TEST_CASE("metrics operation test") {
    RegistryClockSourceStub::reset();
    v1::RegistryT<RegistryClockSourceStub> metrics;

    auto op = metrics.operation(1u, "InsertRemove", "Insert");

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
            "2,InsertRemove.id-1.Insert_bytes,40\n"
            "2,InsertRemove.id-1.Insert_docs,6\n"
            "2,InsertRemove.id-1.Insert_iters,1\n"
            "\n"
            "Gauges\n"
            "\n"
            "Timers\n"
            "2,InsertRemove.id-1.Insert_timer,1\n"
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
