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

#ifndef HEADER_XYZ
#define HEADER_XYZ

#include <bsoncxx/document/value.hpp>

#include <mongocxx/pipeline.hpp>

#include "gennylib/Node.hpp"
#include "gennylib/context.hpp"

#include "value_generators/DocumentGenerator.hpp"

namespace genny {


/**
 * The C++ representation of an aggregation Pipeline which can be specified as an argument for
 * Actors in workloads. It uses DocumentGenerators so the pipeline can be specified with
 * generators like {^RandomInt: ...}.
 */
struct Pipeline {
    /**
     * Converts the given 'pipeline' to an object with numerical property names.
     */
    static bsoncxx::document::value copyPipelineToDocument(const mongocxx::pipeline& pipeline) {
        auto arrayView = pipeline.view_array();
        bsoncxx::document::view docView{arrayView.data(), arrayView.length()};
        return bsoncxx::document::value{docView};
    }

    Pipeline() = default;

    Pipeline(const Node& node, PhaseContext& context, ActorId id) {
        if (!node.isSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("'Pipeline' must be an array"));
        }
        for (auto&& [_, stageNode] : node) {
            stageGenerators.push_back(stageNode.to<DocumentGenerator>(context, id));
        }
    }

    /**
     * Evaluates each 'DocumentGenerator' in 'stageGenerators' and returns the resulting
     * aggregation pipeline.
     */
    mongocxx::pipeline generatePipeline() {
        mongocxx::pipeline pipeline;
        for (auto&& stage : stageGenerators) {
            pipeline.append_stage(stage());
        }

        return pipeline;
    }

    std::vector<DocumentGenerator> stageGenerators;
};


}  // namespace genny

#endif  // HEADER_XYZ
