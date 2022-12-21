// Copyright 2022-present MongoDB Inc.
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

#include <mongocxx/collection.hpp>
#include <mutex>
#include <value_generators/PipelineGenerator.hpp>

namespace genny::actor {

/**
 * This class represents a sample of documents from a collection which is lazily loaded on the first
 * request. It is designed to be shared across threads - it is thread safe.
 *
 * The lazy loading will allow this sample to be taken once the threads are actively running the
 * workload - after previous stages have taken their effect on the collection.
 */
class DeferredSample {
public:
    DeferredSample(std::string actorName,
                   mongocxx::pool::entry client,
                   mongocxx::collection collection,
                   uint sampleSize,
                   PipelineGenerator pipelineSuffixGenerator)
        : _actorName{std::move(actorName)},
          _client(std::move(client)),
          _collection(std::move(collection)),
          _sampleSize(sampleSize),
          _pipelineSuffixGenerator(std::move(pipelineSuffixGenerator)) {}

    /**
     * If this is the first caller, will run an aggregation to gather the sample and return it.
     * Subsequent callers will block until that is finished and then receive a copy of those
     * results.
     */
    std::vector<bsoncxx::document::value> getSample();

private:
    std::vector<bsoncxx::document::value> gatherSample(const std::lock_guard<std::mutex>& lock);

    std::mutex _mutex;

    // This vector lazily loaded, but once populated it is owned here and other threads will receive
    // a view of these documents via a bsoncxx::document::view.
    std::vector<bsoncxx::document::value> _sampleDocs = {};

    std::string _actorName;
    mongocxx::pool::entry _client;
    // '_collection' needs '_client' to be in scope.
    mongocxx::collection _collection;
    uint _sampleSize;
    PipelineGenerator _pipelineSuffixGenerator;
};

/**
 * Given a collection that's already populated, will pull a sample of documents from that
 * collection and then re insert them in order to grow the collection. This is not guaranteed
 * to match the distributions of values in the collection.
 *
 * Owner: query
 */
class SamplingLoader : public Actor {

public:
    SamplingLoader(ActorContext& context,
                   std::string dbName,
                   std::string collectionName,
                   std::shared_ptr<DeferredSample> deferredSamplingLoader);
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
    mongocxx::collection _collection;

    // This is not using WorkloadContext::ShareableState for something that is conceptually similar
    // because we do not want to share the sample across all phases of the workload, which would be
    // a constraint of that system. We want one per phase. We still have to share across different
    // actors for mutliple threads in the same phase, so we do it this way.
    std::shared_ptr<DeferredSample> _deferredSample;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_SAMPLING_LOADER
