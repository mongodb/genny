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

enum class Comparison {
    kGT,
};

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

            if (!req.predicate.evaluateFn(observedValue, req.requiredValue)) {
                BOOST_LOG_TRIVIAL(error)
                    << "Expected metric " << req.predicate.symbol << " " << req.requiredValue
                    << " but actual value was " << observedValue << ".";
                BOOST_THROW_EXCEPTION(
                    PostConditionException("Operation post-condition not granted."));
            }
        }
    }

private:
    struct Predicate {
        std::string key;
        std::string symbol;
        std::function<bool(long long, long long)> evaluateFn;
    };

    struct Requirement {
        MetricRequirement metric;
        const Predicate& predicate;
        long long requiredValue;

        Requirement(MetricRequirement metric, const Predicate& predicate, long long requiredValue)
            : metric(metric), predicate(predicate), requiredValue(requiredValue) {}
    };

    void addCondition(const Node& node) {
        static const char* metricKey = "Metric";
        static const char* documentMetric = "documents";
        static const char* bytesMetric = "bytes";

        static const std::vector<Predicate> predicates = {
            {"EQ", "==", [](long long left, long long right) { return left == right; }},
            {"NE", "!=", [](long long left, long long right) { return left != right; }},
            {"LT", "==", [](long long left, long long right) { return left < right; }},
            {"LTE", "==", [](long long left, long long right) { return left <= right; }},
            {"GT", "==", [](long long left, long long right) { return left > right; }},
            {"GTE", "==", [](long long left, long long right) { return left >= right; }},
        };

        auto metricName = node[metricKey].maybe<std::string>();
        MetricRequirement metric;
        if (!metricName) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "'PostCondition' expects a 'Metric' field of string type."))
        } else if (metricName == documentMetric) {
            metric = MetricRequirement::kNumDocuments;
        } else if (metricName == bytesMetric) {
            metric = MetricRequirement::kBytes;
        } else {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("Unexpected metric"));
        }

        for (const auto& predicate : predicates) {
            if (auto requiredValue = node[predicate.key].maybe<long long>()) {
                requirements.emplace_back(metric, predicate, *requiredValue);
            }
        }
    }

    std::vector<Requirement> requirements;
};

}  // namespace genny

#endif  // HEADER_7CD54FB0_E953_4F3C_B7F4_5BDA6B299878_INCLUDED
