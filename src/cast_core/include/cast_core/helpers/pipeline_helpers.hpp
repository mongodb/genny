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

#ifndef HEADER_608EA64E_00F3_461F_9013_CF60AE6E2602_INCLUDE
#define HEADER_608EA64E_00F3_461F_9013_CF60AE6E2602_INCLUDE

#include <bsoncxx/document/value.hpp>
#include <mongocxx/pipeline.hpp>

#include <value_generators/PipelineGenerator.hpp>

namespace genny::pipeline_helpers {
/**
 * Converts the given 'pipeline' to an object with numerical property names.
 */
bsoncxx::document::value copyPipelineToDocument(const mongocxx::pipeline& pipeline);

/**
 * Instantiates a pipeline from 'pipelineGenerator' and returns a mongocxx::pipeline from the
 * result.
 */
mongocxx::pipeline makePipeline(PipelineGenerator& pipelineGenerator);

}  // namespace genny::pipeline_helpers

#endif  // HEADER_608EA64E_00F3_461F_9013_CF60AE6E2602_INCLUDE
