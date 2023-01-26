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

#ifndef HEADER_7CD54FB0_E953_4F3C_B7F4_5BDA6B299878_INCLUDED
#define HEADER_7CD54FB0_E953_4F3C_B7F4_5BDA6B299878_INCLUDED

#include <gennylib/Node.hpp>

namespace genny {

class PostConditionException : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

enum class MetricRequirement {
    kNumDocuments,
    kBytes,
};

/**
 * A PostCondition added to a CrudActor Operation checks the metrics of the operation immediately
 * after it runs, so that it can be marked as failing if it does not meet expectations for the
 * intended test scenario. For example, a post-condition ensuring that a find command is returning
 * the expected number of documents can quickly identify a spurious performance improvement caused
 * by a bug in query evaluation. A post-condition can also check the state of a collection, ensuring
 * that it is large enough to appropriately stress target code paths, when attached to a query that
 * scans the entire collection.
 *
 * Example:
 * ```yaml
 * Actors:
 * - Name: InsertOne
 *   Type: CrudActor
 *   Database: test
 *   Phases:
 *   - Collection: test
 *     Operations:
 *     - OperationName: insertOne
 *       OperationCommand:
 *         Document: {a: "value"}
 *       PostCondition:
 *       - Metric: documents
 *         EQ: 1
 *       - Metric: bytes
 *         LT: 20
 *         GT: 5
 * ```
 */
struct PostCondition {
    // The default PostCondition is a tautology.
    PostCondition() = default;

    PostCondition(const Node& node) {
        if (node.isSequence()) {
            for (const auto& [k, condition] : node) {
                addCondition(condition);
            }
        } else {
            addCondition(node);
        }
    }

    /**
     * Checks if the 'ops' and 'bytes' metrics for the execution of a CRUD operation meet the
     * requirements and throws an exception if they do not.
     */
    void check(long long ops, long long bytes) const {
        for (const auto& req : requirements) {
            long long observedValue = [&]() {
                switch (req.metric) {
                    case MetricRequirement::kNumDocuments:
                        return ops;
                    case MetricRequirement::kBytes:
                        return bytes;
                }
                throw std::logic_error("impossible");
            }();

            if (!req.relation.evaluateFn(observedValue, req.requiredValue)) {
                BOOST_LOG_TRIVIAL(error)
                    << "Expected metric " << req.relation.symbol << " " << req.requiredValue
                    << " but actual value was " << observedValue << ".";
                BOOST_THROW_EXCEPTION(
                    PostConditionException("Operation post-condition not granted."));
            }
        }
    }

private:
    /**
     * A binary comparison relation on 'long long' ints (e.g., ==, <)
     */
    struct Relation {
        // The YAML key used to include this relation in a PostCondition specification.
        std::string key;

        // The symbol to use when outputting user messages about a comparison result.
        std::string symbol;

        // Implementation that returns true if the relation holds (meeting the post-condition
        // requirement) for a pair of values.
        std::function<bool(long long, long long)> evaluateFn;
    };

    struct Requirement {
        MetricRequirement metric;
        const Relation& relation;
        long long requiredValue;

        Requirement(MetricRequirement metric, const Relation& relation, long long requiredValue)
            : metric(metric), relation(relation), requiredValue(requiredValue) {}
    };

    /**
     * Parse one block from the list of blocks in a YAML PostCondition into one or more entries in
     * the 'requirements' list.
     */
    void addCondition(const Node& node) {
        static const char* metricKey = "Metric";
        static const char* documentMetric = "documents";
        static const char* bytesMetric = "bytes";

        // The parser uses this table to translate the YAML key (e.g., "EQ") for a comparison into a
        // function that can perform the comparison.
        static const std::vector<Relation> relations = {
            {"EQ", "==", [](long long left, long long right) { return left == right; }},
            {"NE", "!=", [](long long left, long long right) { return left != right; }},
            {"LT", "<", [](long long left, long long right) { return left < right; }},
            {"LTE", "<=", [](long long left, long long right) { return left <= right; }},
            {"GT", ">", [](long long left, long long right) { return left > right; }},
            {"GTE", ">=", [](long long left, long long right) { return left >= right; }},
        };

        auto metricName = node[metricKey].maybe<std::string>();
        MetricRequirement metric;
        if (!metricName) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'PostCondition' expects a 'Metric' field of string type."));
        } else if (metricName == documentMetric) {
            metric = MetricRequirement::kNumDocuments;
        } else if (metricName == bytesMetric) {
            metric = MetricRequirement::kBytes;
        } else {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("Unexpected metric"));
        }

        for (const auto& relation : relations) {
            if (auto requiredValue = node[relation.key].maybe<long long>()) {
                requirements.emplace_back(metric, relation, *requiredValue);
            }
        }
    }

    /*
     * A list of requirements that must all be met for the post-condition to be fulfilled. Each
     * requirement specifies the metric to check, a reference value to compare the metric to, and an
     * arithmetic relation (e.g, ==, <) to compare with.
     */
    std::vector<Requirement> requirements;
};

}  // namespace genny

#endif  // HEADER_7CD54FB0_E953_4F3C_B7F4_5BDA6B299878_INCLUDED
