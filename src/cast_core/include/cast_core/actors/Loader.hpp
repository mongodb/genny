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

#ifndef HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A
#define HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A


#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <metrics/metrics.hpp>
#include <value_generators/value_generators.hpp>

namespace genny::actor {

/**
 * Prepares a database for testing. For use with `MultiCollectionUpdate` and
 * `MultiCollectionQuery` actors. It loads a set of documents into multiple collections with
 * indexes. Each collection is identically configured. The document shape, number of documents,
 * number of collections, and list of indexes are all adjustable from the yaml configuration.
 */
class Loader : public Actor {

public:
    explicit Loader(ActorContext& context, uint thread);
    ~Loader() override = default;

    static std::string_view defaultName() {
        return "Loader";
    }
    void run() override;

private:
    /** @private */
    struct PhaseConfig;
    genny::DefaultRandom _rng;
    metrics::Timer _totalBulkLoadTimer;
    metrics::Timer _individualBulkLoadTimer;
    metrics::Timer _indexBuildTimer;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A
