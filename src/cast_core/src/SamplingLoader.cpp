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

#include <cast_core/actors/SamplingLoader.hpp>

#include <algorithm>
#include <iostream>
#include <memory>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/pool.hpp>

#include <boost/log/trivial.hpp>

#include <cast_core/helpers/pipeline_helpers.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/PipelineGenerator.hpp>

namespace genny::actor {

/** @private */
using index_type = std::tuple<DocumentGenerator, std::optional<DocumentGenerator>, std::string>;
using namespace bsoncxx;

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

/** @private */
struct SamplingLoader::PhaseConfig {
    PhaseConfig(PhaseContext& context, mongocxx::pool::entry& client, ActorId id)
        : database{(*client)[context["Database"].to<std::string>()]},
          collection{database[context["Collection"].to<std::string>()]},
          insertBatchSize{context["InsertBatchSize"].to<IntegerSpec>()},
          numBatches{context["Batches"].to<IntegerSpec>()},
          // A sample size is optional. If unspecified, we'll make a sample size such that each
          // sample document is inserted one time.
          sampleSize{
              context["SampleSize"].maybe<IntegerSpec>().value_or(numBatches * insertBatchSize)},
          pipelineSuffixGenerator{context["Pipeline"]
                                      .maybe<PipelineGenerator>(context, id)
                                      .value_or(PipelineGenerator{})} {}

    mongocxx::database database;
    mongocxx::collection collection;
    // See src/workloads/docs/SamplingLoader.yml for a description of the arguments and some
    // examples.
    int64_t insertBatchSize;
    int64_t numBatches;
    int64_t sampleSize;
    PipelineGenerator pipelineSuffixGenerator;
};

std::vector<bsoncxx::document::value> SamplingLoader::gatherSample(PhaseConfig* config) {
    mongocxx::pipeline samplePipeline;
    samplePipeline.sample(config->sampleSize);
    samplePipeline.project(make_document(kvp("_id", 0)));
    const auto suffixPipe = pipeline_helpers::makePipeline(config->pipelineSuffixGenerator);
    samplePipeline.append_stages(suffixPipe.view_array());

    std::vector<bsoncxx::document::value> sampleDocs;
    const int maxRetries = 3;
    for (int nRetries = 0; nRetries < maxRetries; ++nRetries) {
        try {
            auto cursor =
                config->collection.aggregate(samplePipeline, mongocxx::options::aggregate{});
            sampleDocs = std::vector<bsoncxx::document::value>(cursor.begin(), cursor.end());
            break;
        } catch (const mongocxx::operation_exception& ex) {
            if (nRetries >= maxRetries) {
                BOOST_LOG_TRIVIAL(warning)
                    << "Exceeded maximum number of retries: " << maxRetries << ". Giving up";
                BOOST_THROW_EXCEPTION(ex);
            } else if (ex.code().value() == 28799) {
                // $sample couldn't find a non-duplicate document.
                // See SERVER-29446, this can happen sporadically and is safe to retry.
                BOOST_LOG_TRIVIAL(info)
                    << "Got a retryable error when gathering the sample. Retrying...";
            } else {
                BOOST_LOG_TRIVIAL(warning) << "Unexpected error when gathering sample: ";
                BOOST_THROW_EXCEPTION(ex);
            }
        }
    }

    if (sampleDocs.empty()) {
        BOOST_THROW_EXCEPTION(InvalidConfigurationException(
            "Sample was unable to find any documents from collection '" +
            to_string(config->collection.name()) +
            "'. Could the collection be empty or could the pipeline be filtering out "
            "documents? Attempting to sample " +
            boost::to_string(config->sampleSize) +
            " documents. Pipeline suffix = " + to_json(suffixPipe.view_array())));
    }
    if (sampleDocs.size() < config->sampleSize) {
        BOOST_THROW_EXCEPTION(InvalidConfigurationException(
            "Could not get a sample of the expected size. Either the collection '" +
            to_string(config->collection.name()) +
            "' is smaller than the requested sample size of " +
            boost::to_string(config->sampleSize) +
            " documents, or the specified pipeline suffix is filtering documents. Found "
            "only " +
            boost::to_string(sampleDocs.size()) +
            " documents. Pipeline suffix = " + to_json(suffixPipe.view_array())));
    }
    return sampleDocs;
}

void genny::actor::SamplingLoader::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            BOOST_LOG_TRIVIAL(debug) << "Beginning to run SamplingLoader";

            auto sampleDocs = gatherSample(config);

            // Maintain an index into the sample that's used for the insert batches so we
            // insert roughly the same number of copies for each sample.
            size_t sampleIdx = 0;

            // We will re-use the vector of the same size for each batch.
            auto batchOfDocs = std::vector<bsoncxx::document::view_or_value>(
                static_cast<size_t>(config->insertBatchSize));
            auto totalOpCtx = _totalBulkLoad.start();
            for (size_t batch = 0; batch < config->numBatches; ++batch) {
                for (size_t i = 0; i < config->insertBatchSize; ++i) {
                    batchOfDocs[i] = sampleDocs[sampleIdx].view();
                    sampleIdx = (sampleIdx + 1) % sampleDocs.size();
                }

                // Now do the insert.
                auto individualOpCtx = _individualBulkLoad.start();
                try {
                    config->collection.insert_many(batchOfDocs);
                    individualOpCtx.success();
                    totalOpCtx.success();
                } catch (const mongocxx::operation_exception& x) {
                    individualOpCtx.failure();
                    totalOpCtx.failure();
                    BOOST_THROW_EXCEPTION(x);
                }
            }
            BOOST_LOG_TRIVIAL(debug) << "Finished SamplingLoader";
        }
    }
}

SamplingLoader::SamplingLoader(genny::ActorContext& context)
    : Actor(context),
      _totalBulkLoad{context.operation("TotalBulkInsert", SamplingLoader::id())},
      _individualBulkLoad{context.operation("IndividualBulkInsert", SamplingLoader::id())},
      _client{std::move(
          context.client(context.get("ClientName").maybe<std::string>().value_or("Default")))},
      _loop{context, _client, SamplingLoader::id()} {}

namespace {
auto registration = genny::Cast::registerDefault<SamplingLoader>();
}  // namespace
}  // namespace genny::actor
