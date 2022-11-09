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

#ifndef HEADER_SAMPLING_LOADER
#define HEADER_SAMPLING_LOADER


#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/**
 * Given a collection that's already populated, will pull a sample of documents from that
 * collection and then re insert them in order to grow the collection. This is not guaranteed
 * to the distributions of values in the collection.
 *
 * Owner: query
 */
class SamplingLoader : public Actor {

public:
    explicit SamplingLoader(ActorContext& context, uint thread, size_t totalThreads);
    ~SamplingLoader() override = default;

    static std::string_view defaultName() {
        return "SamplingLoader";
    }
    void run() override;

private:
    /** @private */
    struct PhaseConfig;

    metrics::Operation _totalBulkLoad;
    metrics::Operation _individualBulkLoad;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_SAMPLING_LOADER
