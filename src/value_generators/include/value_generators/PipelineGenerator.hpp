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

#ifndef HEADER_5C1488A9_2436_4C4C_922B_352DB2963C5F_INCLUDED
#define HEADER_5C1488A9_2436_4C4C_922B_352DB2963C5F_INCLUDED

#include <gennylib/Node.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny {

/**
 * This generator is just a helper to have a vector of DocumentGenerators which are expected to
 * represent a series of aggregation stages.
 */
struct PipelineGenerator {
    PipelineGenerator() = default;

    PipelineGenerator(const Node& node, ActorContext& context) {
        assertIsArray(node);
        for (auto&& [_, stageNode] : node) {
            // The '1' here is a lie, but we don't necessarily have an actor ID yet in this
            // scenario.
            stageGenerators.push_back(stageNode.to<DocumentGenerator>(context, 1));
        }
    }

    PipelineGenerator(const Node& node, PhaseContext& context, ActorId id) {
        assertIsArray(node);
        for (auto&& [_, stageNode] : node) {
            stageGenerators.push_back(stageNode.to<DocumentGenerator>(context, id));
        }
    }

    std::vector<DocumentGenerator> stageGenerators;

private:
    void assertIsArray(const Node& node) {
        if (!node.isSequence()) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("'Pipeline' must be an array"));
        }
    }
};

}  // namespace genny

#endif  // HEADER_5C1488A9_2436_4C4C_922B_352DB2963C5F_INCLUDED
