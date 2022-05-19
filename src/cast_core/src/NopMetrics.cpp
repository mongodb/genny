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

#include <cast_core/actors/NopMetrics.hpp>

#include <string>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>

namespace genny::actor {

/** @private */
struct NopMetrics::PhaseConfig {
    /** record data about each iteration */
    metrics::Operation operation;

    explicit PhaseConfig(PhaseContext& context, ActorId actorId)
        : operation{context.operation("Iterate", actorId)} {}
};

void NopMetrics::run() {

    for (auto&& config : _loop) {
        auto prev_ctx = std::unique_ptr<metrics::OperationContext>();
        for (auto _ : config) {
            if (prev_ctx) {
                prev_ctx->success();
            }
            prev_ctx = std::make_unique<metrics::OperationContext>(config->operation.start());
        }
        if (prev_ctx) {
            prev_ctx->success();
        }
    }
}

NopMetrics::NopMetrics(genny::ActorContext& context)
    : Actor(context), _loop{context, NopMetrics::id()} {}

namespace {
auto registerNopMetrics = genny::Cast::registerDefault<genny::actor::NopMetrics>();
}
}  // namespace genny::actor
