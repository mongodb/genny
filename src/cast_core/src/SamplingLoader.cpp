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

#include <cast_core/actors/SamplingLoader.hpp>

#include <algorithm>
#include <iostream>
#include <memory>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {

/** @private */
using index_type = std::tuple<DocumentGenerator, std::optional<DocumentGenerator>, std::string>;
using namespace bsoncxx;

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

/** @private */
struct SamplingLoader::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                mongocxx::pool::entry& client,
                uint threadId,
                size_t totalThreads,
                ActorId id)
        : database{(*client)[context["Database"].to<std::string>()]},
          collection{database[context["Collection"].to<std::string>()]},
          // The next line uses integer division for non multithreaded configurations.
          // The Remainder is accounted for below.
          insertBatchSize{context["InsertBatchSize"].to<IntegerSpec>()},
          numBatches{context["Batches"].to<IntegerSpec>()},
          sampleSize{
              context["SampleSize"].maybe<IntegerSpec>().value_or(numBatches * insertBatchSize)},
          threadId(threadId) {}

    mongocxx::database database;
    mongocxx::collection collection;
    int64_t insertBatchSize;
    int64_t numBatches;
    int64_t sampleSize;
    uint threadId;
};

void genny::actor::SamplingLoader::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            BOOST_LOG_TRIVIAL(info) << "Beginning to run SamplingLoader";
            // Read the sample size documents into a vector.
            mongocxx::pipeline pipe;
            pipe.sample(config->sampleSize);
            pipe.project(make_document(kvp("_id", 0)));
            auto cursor = config->collection.aggregate(pipe, mongocxx::options::aggregate{});
            const auto sampleDocs =
                std::vector<bsoncxx::document::value>(cursor.begin(), cursor.end());

            if (sampleDocs.empty()) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException("Collection has no documents"));
            }
            if (sampleDocs.size() < config->sampleSize) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("Collection smaller than sample size"));
            }

            // Maintain an index into the sample that's used for the insert batches so we
            // insert roughly the same number of copies for each sample.
            size_t sampleIdx = 0;

            auto batchOfDocs = std::vector<bsoncxx::document::view_or_value>{};
            auto totalOpCtx = _totalBulkLoad.start();
            for (size_t batch = 0; batch < config->numBatches; ++batch) {
                batchOfDocs.clear();
                for (size_t i = 0; i < config->insertBatchSize; ++i) {
                    batchOfDocs.push_back(sampleDocs[sampleIdx].view());
                    sampleIdx = (sampleIdx + 1) % sampleDocs.size();
                }

                // Now do the insert.
                auto individualOpCtx = _individualBulkLoad.start();
                try {
                    auto result = config->collection.insert_many(std::move(batchOfDocs));
                    individualOpCtx.success();
                    totalOpCtx.success();
                } catch (const mongocxx::operation_exception& x) {
                    individualOpCtx.failure();
                    totalOpCtx.failure();
                    BOOST_THROW_EXCEPTION(x);
                }
            }
            BOOST_LOG_TRIVIAL(info) << "Finished SamplingLoader on thread " << config->threadId;
        }
    }
}

SamplingLoader::SamplingLoader(genny::ActorContext& context, uint thread, size_t totalThreads)
    : Actor(context),
      _totalBulkLoad{context.operation("TotalBulkInsert", SamplingLoader::id())},
      _individualBulkLoad{context.operation("IndividualBulkInsert", SamplingLoader::id())},
      _client{std::move(
          context.client(context.get("ClientName").maybe<std::string>().value_or("Default")))},
      _loop{context, _client, thread, totalThreads, SamplingLoader::id()} {}

class SamplingLoaderProducer : public genny::ActorProducer {
public:
    SamplingLoaderProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context["Type"].to<std::string>() != "SamplingLoader") {
            return {};
        }
        genny::ActorVector out;
        uint totalThreads = context["Threads"].to<int>();

        for (uint i = 0; i < totalThreads; ++i) {
            out.emplace_back(
                std::make_unique<genny::actor::SamplingLoader>(context, i, totalThreads));
        }
        return out;
    }
};

namespace {
std::shared_ptr<genny::ActorProducer> loaderProducer =
    std::make_shared<SamplingLoaderProducer>("SamplingLoader");
auto registration = genny::Cast::registerCustom<genny::ActorProducer>(loaderProducer);
}  // namespace
}  // namespace genny::actor
