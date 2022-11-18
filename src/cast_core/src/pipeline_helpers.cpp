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

#include <cast_core/helpers/pipeline_helpers.hpp>

namespace genny::pipeline_helpers {
/**
 * Converts the given 'pipeline' to an object with numerical property names.
 */
bsoncxx::document::value copyPipelineToDocument(const mongocxx::pipeline& pipeline) {
    auto arrayView = pipeline.view_array();
    bsoncxx::document::view docView{arrayView.data(), arrayView.length()};
    return bsoncxx::document::value{docView};
}

/**
 * Instantiates a pipeline from 'pipelineGenerator' and returns a mongocxx::pipeline from the
 * result.
 */
mongocxx::pipeline makePipeline(PipelineGenerator& pipelineGenerator) {
    mongocxx::pipeline pipeline;
    for (auto&& stageGen : pipelineGenerator.stageGenerators) {
        pipeline.append_stage(stageGen());
    }

    return pipeline;
}

}  // namespace genny::pipeline_helpers
