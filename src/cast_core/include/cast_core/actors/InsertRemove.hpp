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

#ifndef HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
#define HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED

#include <iostream>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/operation.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/**
 * InsertRemove is a simple actor that inserts and then removes the same document from a
 * collection. It uses `PhaseLoop` for looping.  Each instance of the actor uses a
 * different document, indexed by an integer `_id` field. The actor records the latency of each
 * insert and each remove.
 *
 * Owner: product-perf
 */
class InsertRemove : public Actor {

public:
    explicit InsertRemove(ActorContext& context);
    ~InsertRemove() override = default;

    static std::string_view defaultName() {
        return "InsertRemove";
    }
    void run() override;

private:
    mongocxx::pool::entry _client;
    DefaultRandom& _rng;

    metrics::Operation _insert;
    metrics::Operation _remove;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_A5170346_CB57_4438_854F_20C3D99FF187_INCLUDED
