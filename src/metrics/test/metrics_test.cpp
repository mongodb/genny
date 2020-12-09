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
#include <optional>

#include <google/protobuf/util/message_differencer.h>

#include <metrics/MetricsReporter.hpp>
#include <metrics/metrics.hpp>
#include <metrics/v2/event.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/clocks.hpp>
#include <testlib/helpers.hpp>


namespace genny::metrics {

namespace internals::v2 {

/**
 * Very basic mock for tests.
 */
class MockStreamInterface {
public:
    MockStreamInterface(const std::string& name, const ActorId& actorId) {}

    void write(const poplar::EventMetrics& event) {
        events.push_back(event);
    }

    void finish() {}

    // We make this static so we can access it even several private objects deep.
    static std::vector<poplar::EventMetrics> events;
};

}  // namespace internals::v2

std::vector<poplar::EventMetrics> internals::v2::MockStreamInterface::events;

namespace {

using namespace std::literals::chrono_literals;
using namespace genny::testing;


void assertDurationsEqual(RegistryClockSourceStub::duration dur1,
                          RegistryClockSourceStub::duration dur2) {
    // We compare std:chrono::time_points by converting through Period in order to take advantage of
    // its operator<<(std::ostream&) in case the check fails.
    REQUIRE(Period<RegistryClockSourceStub>{dur1} == Period<RegistryClockSourceStub>{dur2});
}


TEST_CASE("metrics::OperationContext interface") {
    RegistryClockSourceStub::reset();

    auto dummy_metrics = internals::RegistryT<RegistryClockSourceStub>{};
    auto op =
        internals::OperationImpl<RegistryClockSourceStub>{"Actor", dummy_metrics, "Op", nullptr};

    RegistryClockSourceStub::advance(5ns);
    auto ctx = std::make_optional<internals::OperationContextT<RegistryClockSourceStub>>(&op);

    ctx->addDocuments(200);
    ctx->addBytes(3000);
    ctx->addErrors(4);
    RegistryClockSourceStub::advance(67ns);

    REQUIRE(op.getEvents().size() == 0);

    auto expected = OperationEventT<RegistryClockSourceStub>{};
    expected.ops = 1;
    expected.number = 200;
    expected.size = 3000;
    expected.errors = 4;

    SECTION("success() reports the operation") {
        expected.duration = 67ns;
        ctx->success();
        REQUIRE(op.getEvents().size() == 1);

        ctx.reset();

        expected.outcome = OutcomeType::kSuccess;
        assertDurationsEqual(op.getEvents()[0].first.time_since_epoch(), 72ns);
        REQUIRE(op.getEvents()[0].second == expected);
    }

    SECTION("failure() reports the operation") {
        expected.duration = 67ns;
        ctx->failure();
        REQUIRE(op.getEvents().size() == 1);

        ctx.reset();

        expected.outcome = OutcomeType::kFailure;
        assertDurationsEqual(op.getEvents()[0].first.time_since_epoch(), 72ns);
        REQUIRE(op.getEvents()[0].second == expected);
    }

    SECTION("discard() doesn't report the operation") {
        ctx.reset();
        REQUIRE(op.getEvents().size() == 0);
    }

    SECTION("add*() methods can be called multiple times") {
        ctx->addIterations(8);
        ctx->addIterations(9);
        ctx->addDocuments(200);
        ctx->addBytes(3000);
        ctx->addErrors(4);
        RegistryClockSourceStub::advance(67ns);

        REQUIRE(op.getEvents().size() == 0);

        expected.ops = 17;
        expected.number += 200;
        expected.size += 3000;
        expected.errors += 4;

        expected.duration = 134ns;
        ctx->success();
        REQUIRE(op.getEvents().size() == 1);

        ctx.reset();

        expected.outcome = OutcomeType::kSuccess;
        assertDurationsEqual(op.getEvents()[0].first.time_since_epoch(), 139ns);
        REQUIRE(op.getEvents()[0].second == expected);
    }
}

TEST_CASE("metrics output format") {
    RegistryClockSourceStub::reset();
    auto metrics = internals::RegistryT<RegistryClockSourceStub>{};
    auto reporter = genny::metrics::internals::v1::ReporterT{metrics};

    //           +---------------------+----------------+
    // Thread 1: |        Insert       |     Remove     |
    //           +---------------------+----------------+
    //                +------------------+  +--------+
    // Thread 2:      |      Insert      |  | Remove |
    //                +------------------+  +--------+
    //                   +-----------+
    // Thread 3:         | Greetings |
    //                   +-----------+

    auto insert1 = metrics.operation("InsertRemove", "Insert", 1u);
    auto insert2 = metrics.operation("InsertRemove", "Insert", 2u);
    auto remove1 = metrics.operation("InsertRemove", "Remove", 1u);
    auto remove2 = metrics.operation("InsertRemove", "Remove", 2u);
    auto greetings3 = metrics.operation("HelloWorld", "Greetings", 3u);
    auto synthetic = metrics.operation("HelloWorld", "Synthetic", 4u);

    RegistryClockSourceStub::advance(5ns);
    synthetic.report(RegistryClockSourceStub::now(),
                     std::chrono::microseconds{300},
                     OutcomeType::kSuccess,
                     3,
                     2,
                     1,
                     4);

    auto insert1Ctx = insert1.start();

    RegistryClockSourceStub::advance(5ns);
    auto insert2Ctx = insert2.start();

    RegistryClockSourceStub::advance(3ns);
    auto greetings3Ctx = greetings3.start();

    RegistryClockSourceStub::advance(13ns);
    greetings3Ctx.addIterations(2);
    greetings3Ctx.success();

    RegistryClockSourceStub::advance(2ns);
    insert1Ctx.addDocuments(9);
    insert1Ctx.addBytes(300);
    insert1Ctx.success();
    auto remove1Ctx = remove1.start();

    RegistryClockSourceStub::advance(2ns);
    insert2Ctx.addDocuments(8);
    insert2Ctx.addBytes(200);
    insert2Ctx.success();

    RegistryClockSourceStub::advance(2ns);
    auto remove2Ctx = remove2.start();

    RegistryClockSourceStub::advance(10ns);
    remove2Ctx.addDocuments(7);
    remove2Ctx.addBytes(30);
    remove2Ctx.success();

    RegistryClockSourceStub::advance(3ns);
    remove1Ctx.addDocuments(6);
    remove1Ctx.addBytes(40);
    remove1Ctx.success();

    SECTION("csv reporting") {
        auto expected =
            "Clocks\n"
            "SystemTime,42000000\n"
            "MetricsTime,45\n"
            "\n"
            "Counters\n"
            "5,HelloWorld.id-4.Synthetic_bytes,4\n"
            "26,HelloWorld.id-3.Greetings_bytes,0\n"
            "42,InsertRemove.id-2.Remove_bytes,30\n"
            "45,InsertRemove.id-1.Remove_bytes,40\n"
            "30,InsertRemove.id-2.Insert_bytes,200\n"
            "28,InsertRemove.id-1.Insert_bytes,300\n"
            "5,HelloWorld.id-4.Synthetic_docs,1\n"
            "26,HelloWorld.id-3.Greetings_docs,0\n"
            "42,InsertRemove.id-2.Remove_docs,7\n"
            "45,InsertRemove.id-1.Remove_docs,6\n"
            "30,InsertRemove.id-2.Insert_docs,8\n"
            "28,InsertRemove.id-1.Insert_docs,9\n"
            "5,HelloWorld.id-4.Synthetic_iters,3\n"
            "26,HelloWorld.id-3.Greetings_iters,2\n"
            "42,InsertRemove.id-2.Remove_iters,1\n"
            "45,InsertRemove.id-1.Remove_iters,1\n"
            "30,InsertRemove.id-2.Insert_iters,1\n"
            "28,InsertRemove.id-1.Insert_iters,1\n"
            "\n"
            "Gauges\n"
            "\n"
            "Timers\n"
            "5,HelloWorld.id-4.Synthetic_timer,300000\n"
            "26,HelloWorld.id-3.Greetings_timer,13\n"
            "42,InsertRemove.id-2.Remove_timer,10\n"
            "45,InsertRemove.id-1.Remove_timer,17\n"
            "30,InsertRemove.id-2.Insert_timer,20\n"
            "28,InsertRemove.id-1.Insert_timer,23\n"
            "\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, MetricsFormat("csv"));
        REQUIRE(out.str() == expected);
    }

    SECTION("cedar-csv reporting") {
        auto expected =
            "Clocks\n"
            "clock,nanoseconds\n"
            "SystemTime,42000000\n"
            "MetricsTime,45\n"
            "\n"
            "OperationThreadCounts\n"
            "actor,operation,workers\n"
            "HelloWorld,Greetings,1\n"
            "HelloWorld,Synthetic,1\n"
            "InsertRemove,Insert,2\n"
            "InsertRemove,Remove,2\n"
            "\n"
            "Operations\n"
            "timestamp,actor,thread,operation,duration,outcome,n,ops,errors,size\n"
            "5,HelloWorld,4,Synthetic,300000,0,1,3,2,4\n"
            "26,HelloWorld,3,Greetings,13,0,0,2,0,0\n"
            "42,InsertRemove,2,Remove,10,0,7,1,0,30\n"
            "45,InsertRemove,1,Remove,17,0,6,1,0,40\n"
            "30,InsertRemove,2,Insert,20,0,8,1,0,200\n"
            "28,InsertRemove,1,Insert,23,0,9,1,0,300\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, MetricsFormat("cedar-csv"));
        REQUIRE(out.str() == expected);
    }
}

TEST_CASE("Genny.Setup metric") {
    RegistryClockSourceStub::reset();
    auto metrics = internals::RegistryT<RegistryClockSourceStub>{};
    auto reporter = genny::metrics::internals::v1::ReporterT{metrics};

    // Mimic what the DefaultDriver would be doing.
    auto setup = metrics.operation("Genny", "Setup", 0u);

    RegistryClockSourceStub::advance(5ns);
    auto ctx = setup.start();

    RegistryClockSourceStub::advance(10ns);
    ctx.success();

    SECTION("csv reporting should report it as a timer") {
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
        reporter.report<ReporterClockSourceStub>(out, MetricsFormat("csv"));
        REQUIRE(out.str() == expected);
    }

    SECTION("cedar-csv reporting should report it") {
        auto expected =
            "Clocks\n"
            "clock,nanoseconds\n"
            "SystemTime,42000000\n"
            "MetricsTime,15\n"
            "\n"
            "OperationThreadCounts\n"
            "actor,operation,workers\n"
            "Genny,Setup,1\n"
            "\n"
            "Operations\n"
            "timestamp,actor,thread,operation,duration,outcome,n,ops,errors,size\n"
            "15,Genny,0,Setup,10,0,0,1,0,0\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, MetricsFormat("cedar-csv"));
        REQUIRE(out.str() == expected);
    }
}

TEST_CASE("Genny.ActiveActors metric") {
    RegistryClockSourceStub::reset();
    auto metrics = internals::RegistryT<RegistryClockSourceStub>{};
    auto reporter = genny::metrics::internals::v1::ReporterT{metrics};

    // Mimic what the DefaultDriver would be doing.
    auto startedActors = metrics.operation("Genny", "ActorStarted", 0u);
    auto finishedActors = metrics.operation("Genny", "ActorFinished", 0u);

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

    SECTION("csv reporting should report it as a counter") {
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
        reporter.report<ReporterClockSourceStub>(out, MetricsFormat("csv"));
        REQUIRE(out.str() == expected);
    }

    SECTION("cedar-csv reporting shouldn't report it") {
        auto expected =
            "Clocks\n"
            "clock,nanoseconds\n"
            "SystemTime,42000000\n"
            "MetricsTime,385\n"
            "\n"
            "OperationThreadCounts\n"
            "actor,operation,workers\n"
            "\n"
            "Operations\n"
            "timestamp,actor,thread,operation,duration,outcome,n,ops,errors,size\n";

        std::ostringstream out;
        reporter.report<ReporterClockSourceStub>(out, MetricsFormat("cedar-csv"));
        REQUIRE(out.str() == expected);
    }
}

TEST_CASE("Operation with threshold") {

    auto setup = []() {
        RegistryClockSourceStub::reset();
        auto metrics = internals::RegistryT<RegistryClockSourceStub>{};

        genny::metrics::internals::v1::ReporterT{metrics};

        return metrics;
    };

    auto runActor = [](internals::OperationT<RegistryClockSourceStub>& actor,
                       std::chrono::nanoseconds advance) {
        auto ctx = actor.start();
        RegistryClockSourceStub::advance(advance);
        ctx.success();
    };

    SECTION("50% threshold") {
        auto metrics = setup();
        auto actor = metrics.operation("MyActor", "MyOp", 0u, TimeSpec(10), 50.0);

        runActor(actor, 1ns);
        runActor(actor, 1ns);
        runActor(actor, 51ns);
        runActor(actor, 51ns);
        REQUIRE_THROWS_AS(runActor(actor, 51ns), internals::OperationThresholdExceededException);
    }

    SECTION("100% threshold") {
        auto metrics = setup();
        auto actor = metrics.operation("MyActor", "MyOp", 0u, TimeSpec(10), 100.0);

        runActor(actor, 9999ns);
        runActor(actor, 9999ns);
        runActor(actor, 9999ns);
        runActor(actor, 9999ns);
    }

    SECTION("0% threshold") {
        auto metrics = setup();
        auto actor = metrics.operation("MyActor", "MyOp", 0u, TimeSpec(10), 0.0);

        runActor(actor, 1ns);
        runActor(actor, 1ns);
        runActor(actor, 1ns);
        runActor(actor, 1ns);
        runActor(actor, 1ns);
        runActor(actor, 1ns);
        runActor(actor, 1ns);
        REQUIRE_THROWS_AS(runActor(actor, 11ns), internals::OperationThresholdExceededException);
    }
}

TEST_CASE("Phases can set metrics") {

    SECTION("With MetricsName") {
        NodeSource yaml(R"(
        SchemaVersion: 2018-07-01
        Database: test
        Actors:
        - Name: MetricsNameTest
          Type: HelloWorld
          Threads: 1
          Phases:
          - Repeat: 1
            MetricsName: Phase1Metrics

        Metrics:
          Format: cedar-csv
          Path: build/genny-metrics
        )",
                        "");

        ActorHelper ah{yaml.root(), 1};
        ah.run();

        const auto metrics = ah.getMetricsOutput();
        REQUIRE_THAT(std::string(metrics), Catch::Contains("Phase1Metrics_bytes,13"));
    }

    SECTION("With default metrics name") {
        NodeSource yaml(R"(
    SchemaVersion: 2018-07-01
    Database: test
    Actors:
    - Name: MetricsNameTest
      Type: HelloWorld
      Threads: 1
      Phases:
      - Repeat: 1

    Metrics:
      Format: cedar-csv
      Path: build/genny-metrics
    )",
                        "");

        ActorHelper ah{yaml.root(), 1};
        ah.run();

        const auto metrics = ah.getMetricsOutput();
        REQUIRE_THAT(std::string(metrics), Catch::Contains("DefaultMetricsName.0_bytes,13"));
    }
}

TEST_CASE("Registry counts the number of workers") {
    RegistryClockSourceStub::reset();
    auto metrics = internals::RegistryT<RegistryClockSourceStub>{};

    metrics.operation("actor1", "op1", 1u);
    metrics.operation("actor1", "op1", 2u);
    metrics.operation("actor1", "op1", 3u);

    metrics.operation("actor2", "op1", 4u);
    metrics.operation("actor2", "op1", 5u);

    std::size_t expected1 = 3;
    std::size_t expected2 = 2;

    REQUIRE(metrics.getWorkerCount("actor1", "op1") == 3);
    REQUIRE(metrics.getWorkerCount("actor2", "op1") == 2);
}


/**
 * These tests work for the happy cases, but after we remove legacy
 * CSV formats we should make the pieces of the metrics API more
 * unit-testable. Examples of things to test:
 * - What happens to the metrics if an exception is thrown while
 *   an operation context is open?
 * - What happens if there's a gRPC error?
 */
TEST_CASE("Events stream to gRPC") {
    using EventVec = std::vector<poplar::EventMetrics>;

    auto compareEventsAndClear = [](const EventVec& eventsIn) {
        internals::v2::MockStreamInterface interface("dummyDebugName", 5);
        REQUIRE(eventsIn.size() == interface.events.size());
        for (int i = 0; i < eventsIn.size(); i++) {
            REQUIRE(google::protobuf::util::MessageDifferencer::Equals(eventsIn[i],
                                                                       interface.events[i]));
        };
        interface.events.clear();
    };

    auto getMetricsPath = []() {
        boost::filesystem::path ph =
            boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
        return (ph / "genny-metrics").string();
    };


    SECTION("Empty stream") {

        RegistryClockSourceStub::reset();
        auto stream =
            internals::v2::EventStream<RegistryClockSourceStub, internals::v2::MockStreamInterface>{
                1, "TestName", 1};
        REQUIRE_FALSE(stream.sendOne(true));

        EventVec expected;
        compareEventsAndClear(expected);
    }

    SECTION("Three events") {

        RegistryClockSourceStub::reset();
        auto stream =
            internals::v2::EventStream<RegistryClockSourceStub, internals::v2::MockStreamInterface>{
                1, "EventName", 9};
        EventVec expected;


        // First Event
        RegistryClockSourceStub::advance(std::chrono::microseconds(5));
        {
            OperationEventT<RegistryClockSourceStub> event(
                2,                                                              // number
                3,                                                              // ops
                7,                                                              // size
                0,                                                              // errors
                Period<RegistryClockSourceStub>{std::chrono::microseconds(5)},  // duration
                OutcomeType::kSuccess                                           // outcome
            );
            stream.addAt(RegistryClockSourceStub::now(), event, 1);
            REQUIRE(stream.sendOne(true));

            // Streamed Event
            // ---------------
            // Expected event

            poplar::EventMetrics metric;
            metric.set_name("EventName");
            metric.set_id(1);

            metric.mutable_time()->set_seconds(0);
            metric.mutable_time()->set_nanos(5000);

            metric.mutable_timers()->mutable_duration()->set_seconds(0);
            metric.mutable_timers()->mutable_duration()->set_nanos(5000);
            metric.mutable_timers()->mutable_total()->set_seconds(0);
            metric.mutable_timers()->mutable_total()->set_nanos(5000);

            metric.mutable_counters()->set_number(2);
            metric.mutable_counters()->set_ops(3);
            metric.mutable_counters()->set_size(7);
            metric.mutable_counters()->set_errors(0);

            metric.mutable_gauges()->set_failed(false);
            metric.mutable_gauges()->set_workers(1);
            metric.mutable_gauges()->set_state(9);

            expected.push_back(metric);
        }

        // Second Event
        RegistryClockSourceStub::advance(std::chrono::microseconds(7));
        {
            OperationEventT<RegistryClockSourceStub> event(
                2,                                                              // number
                3,                                                              // ops
                90,                                                             // size
                1,                                                              // errors
                Period<RegistryClockSourceStub>{std::chrono::microseconds(6)},  // duration
                OutcomeType::kFailure                                           // outcome
            );
            stream.addAt(RegistryClockSourceStub::now(), event, 1);
            // Drive-by test that we won't send when the buffer is under capacity.
            REQUIRE_FALSE(stream.sendOne());
            REQUIRE(stream.sendOne(true));

            // Streamed Event
            // ---------------
            // Expected event

            poplar::EventMetrics metric;
            metric.set_name("EventName");
            metric.set_id(1);

            metric.mutable_time()->set_seconds(0);
            metric.mutable_time()->set_nanos(12000);

            metric.mutable_timers()->mutable_duration()->set_seconds(0);
            metric.mutable_timers()->mutable_duration()->set_nanos(6000);
            metric.mutable_timers()->mutable_total()->set_seconds(0);
            metric.mutable_timers()->mutable_total()->set_nanos(7000);

            metric.mutable_counters()->set_number(2);
            metric.mutable_counters()->set_ops(3);
            metric.mutable_counters()->set_size(90);
            metric.mutable_counters()->set_errors(1);

            metric.mutable_gauges()->set_failed(true);
            metric.mutable_gauges()->set_workers(1);
            metric.mutable_gauges()->set_state(9);

            expected.push_back(metric);
        }

        // Third Event
        RegistryClockSourceStub::advance(std::chrono::seconds(9));
        RegistryClockSourceStub::advance(std::chrono::microseconds(3));
        {
            OperationEventT<RegistryClockSourceStub> event(
                3,                                                              // number
                1,                                                              // ops
                10,                                                             // size
                0,                                                              // errors
                Period<RegistryClockSourceStub>{std::chrono::microseconds(3)},  // duration
                OutcomeType::kSuccess                                           // outcome
            );
            stream.addAt(RegistryClockSourceStub::now(), event, 1);
            REQUIRE(stream.sendOne(true));

            // Streamed Event
            // ---------------
            // Expected event

            poplar::EventMetrics metric;
            metric.set_name("EventName");
            metric.set_id(1);

            metric.mutable_time()->set_seconds(9);
            metric.mutable_time()->set_nanos(15000);

            metric.mutable_timers()->mutable_duration()->set_seconds(0);
            metric.mutable_timers()->mutable_duration()->set_nanos(3000);
            metric.mutable_timers()->mutable_total()->set_seconds(9);
            metric.mutable_timers()->mutable_total()->set_nanos(3000);

            metric.mutable_counters()->set_number(3);
            metric.mutable_counters()->set_ops(1);
            metric.mutable_counters()->set_size(10);
            metric.mutable_counters()->set_errors(0);

            metric.mutable_gauges()->set_failed(false);
            metric.mutable_gauges()->set_workers(1);
            metric.mutable_gauges()->set_state(9);

            expected.push_back(metric);
        }


        compareEventsAndClear(expected);
    }

    SECTION("Accept events created after end time") {
        RegistryClockSourceStub::reset();

        RegistryClockSourceStub::advance(std::chrono::microseconds(7));
        auto endTime = RegistryClockSourceStub::now();

        RegistryClockSourceStub::advance(std::chrono::microseconds(3));

        auto stream =
            internals::v2::EventStream<RegistryClockSourceStub, internals::v2::MockStreamInterface>{
                3,
                "LateEventName",
                1,
            };
        EventVec expected;
        {
            OperationEventT<RegistryClockSourceStub> event(
                0,                                                              // number
                77,                                                             // ops
                2,                                                              // size
                0,                                                              // errors
                Period<RegistryClockSourceStub>{std::chrono::microseconds(6)},  // duration
                OutcomeType::kSuccess                                           // outcome
            );
            stream.addAt(endTime, event, 1);
            REQUIRE(stream.sendOne(true));

            // Streamed Event
            // ---------------
            // Expected event

            poplar::EventMetrics metric;
            metric.set_name("LateEventName");
            metric.set_id(3);

            metric.mutable_time()->set_seconds(0);
            metric.mutable_time()->set_nanos(7000);

            metric.mutable_timers()->mutable_duration()->set_seconds(0);
            metric.mutable_timers()->mutable_duration()->set_nanos(6000);
            metric.mutable_timers()->mutable_total()->set_seconds(0);
            metric.mutable_timers()->mutable_total()->set_nanos(6000);

            metric.mutable_counters()->set_number(0);
            metric.mutable_counters()->set_ops(77);
            metric.mutable_counters()->set_size(2);
            metric.mutable_counters()->set_errors(0);

            metric.mutable_gauges()->set_failed(false);
            metric.mutable_gauges()->set_workers(1);
            metric.mutable_gauges()->set_state(1);

            expected.push_back(metric);
        }

        compareEventsAndClear(expected);
    }


    SECTION("Create folder for ftdc output") {
        auto metricsPath = getMetricsPath();

        REQUIRE(!boost::filesystem::exists(metricsPath));
        auto ftdc_metrics =
            internals::RegistryT<RegistryClockSourceStub>{MetricsFormat("ftdc"), metricsPath};
        REQUIRE(boost::filesystem::exists(metricsPath));

        auto neverConstructedMetricsPath = getMetricsPath();

        REQUIRE(!boost::filesystem::exists(neverConstructedMetricsPath));
        auto csv_metrics =
            internals::RegistryT<RegistryClockSourceStub>{MetricsFormat("csv"), metricsPath};
        REQUIRE(!boost::filesystem::exists(neverConstructedMetricsPath));

        REQUIRE(boost::filesystem::remove_all(metricsPath));
    }

    SECTION("One Collector is created per actor-operation.") {
        auto metricsPath = getMetricsPath();
        auto metrics =
            internals::RegistryT<RegistryClockSourceStub>{MetricsFormat("ftdc"), metricsPath};

        metrics.operation("dummyActorName", "dummyOpName", 1, 2);
        metrics.operation("dummyActorName", "dummyOpName", 2, 2);
        metrics.operation("dummyActorName", "dummyOpName", 2, 3);
        metrics.operation("dummyActorName", "anotherDummyOpName", 2, 4);

        REQUIRE(boost::filesystem::exists(metricsPath + "/dummyActorName.dummyOpName.2.ftdc"));
        REQUIRE(boost::filesystem::exists(metricsPath + "/dummyActorName.dummyOpName.3.ftdc"));
        REQUIRE(
            boost::filesystem::exists(metricsPath + "/dummyActorName.anotherDummyOpName.4.ftdc"));

        int file_count = 0;
        for (boost::filesystem::directory_iterator it(metricsPath);
             it != boost::filesystem::directory_iterator();
             ++it) {
            file_count++;
        }

        REQUIRE(file_count == 4);

        REQUIRE(boost::filesystem::remove_all(metricsPath));
    }

    SECTION("Un-forced metrics buffer only pops at capacity.") {
        auto metricsBuffer =
            internals::v2::MetricsBuffer<RegistryClockSourceStub>(16, "test_buffer");
        auto endTime = RegistryClockSourceStub::now();
        OperationEventT<RegistryClockSourceStub> event(
            2,                                                              // number
            77,                                                             // ops
            2,                                                              // size
            0,                                                              // errors
            Period<RegistryClockSourceStub>{std::chrono::microseconds(6)},  // duration
            OutcomeType::kSuccess                                           // outcome
        );

        for (int i = 0; i < 6; i++) {
            metricsBuffer.addAt(endTime, event, 1);
            REQUIRE_FALSE(metricsBuffer.pop(false));
        }
        metricsBuffer.addAt(endTime, event, 1);
        REQUIRE(metricsBuffer.pop(false));
    }

    SECTION("Metrics buffer throws when capacity exceeded.") {
        auto metricsBuffer =
            internals::v2::MetricsBuffer<RegistryClockSourceStub>(3, "test_buffer");
        auto endTime = RegistryClockSourceStub::now();
        OperationEventT<RegistryClockSourceStub> event(
            2,                                                              // number
            77,                                                             // ops
            2,                                                              // size
            0,                                                              // errors
            Period<RegistryClockSourceStub>{std::chrono::microseconds(6)},  // duration
            OutcomeType::kSuccess                                           // outcome
        );

        metricsBuffer.addAt(endTime, event, 1);
        metricsBuffer.addAt(endTime, event, 1);
        metricsBuffer.addAt(endTime, event, 1);
        metricsBuffer.addAt(endTime, event, 1);
        REQUIRE_THROWS(metricsBuffer.pop(false));
    }
}

}  // namespace
}  // namespace genny::metrics
