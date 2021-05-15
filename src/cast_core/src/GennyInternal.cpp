// Copyright 2021-present MongoDB Inc.
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

#include <cast_core/actors/GennyInternal.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

void GennyInternal::run() {

    std::optional<metrics::clock::time_point> startTime;
    metrics::clock::time_point finishTime;

    // We don't use PhaseLoop because we want this actor to be usable
    // in any workload regardless of number of phases defined.
    while (_orchestrator.morePhases()) {
        _orchestrator.awaitPhaseStart();
        if (startTime) { // If this isn't the first loop.
            reportPhase(*startTime, finishTime);
        }
        startTime = metrics::clock::now();
        _orchestrator.awaitPhaseEnd();

    }
    reportPhase(*startTime, finishTime);
}

GennyInternal::GennyInternal(genny::ActorContext& context)
    : Actor{context},
      _phaseOp{context.operation("Phase", GennyInternal::id())},
      _orchestrator{context.orchestrator()} {}

void GennyInternal::reportPhase(metrics::clock::time_point startTime, metrics::clock::time_point finishTime) {
    finishTime = metrics::clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(finishTime - startTime);
    _phaseOp.report(std::move(finishTime), std::move(duration), metrics::OutcomeType::kSuccess);
}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
auto registerGennyInternal = Cast::registerDefault<GennyInternal>();
}  // namespace
}  // namespace genny::actor
