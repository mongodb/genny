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

#ifndef HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED
#define HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED

#include <string_view>

#include <mongocxx/database.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace YAML {

template <>
struct convert<mongocxx::options::aggregate> {
    using AggregateOptions = mongocxx::options::aggregate;
    static Node encode(const AggregateOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, AggregateOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        rhs = mongocxx::options::aggregate{};
        if (node["AllowDiskUse"]) {
            auto allowDiskUse = node["AllowDiskUse"].as<bool>();
            rhs.allow_disk_use(allowDiskUse);
        }
        if (node["BatchSize"]) {
            auto batchSize = node["BatchSize"].as<int>();
            rhs.batch_size(batchSize);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds(maxTime));
        }
        if (node["ReadPreference"]) {
            auto readPreference = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPreference);
        }
        if (node["BypassDocumentValidation"]) {
            auto bypassValidation = node["BypassDocumentValidation"].as<bool>();
            rhs.bypass_document_validation(bypassValidation);
        }
        if (node["Hint"]) {
            auto h = node["Hint"].as<std::string>();
            auto hint = mongocxx::hint(h);
            rhs.hint(hint);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::bulk_write> {
    using BulkWriteOptions = mongocxx::options::bulk_write;
    static Node encode(const BulkWriteOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, BulkWriteOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["BypassDocumentValidation"]) {
            auto bypassDocValidation = node["BypassDocumentValidation"].as<bool>();
            rhs.bypass_document_validation(bypassDocValidation);
        }
        if (node["Ordered"]) {
            auto isOrdered = node["Ordered"].as<bool>();
            rhs.ordered(isOrdered);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::count> {
    using CountOptions = mongocxx::options::count;
    static Node encode(const CountOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, CountOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["Hint"]) {
            auto h = node["Hint"].as<std::string>();
            auto hint = mongocxx::hint(h);
            rhs.hint(hint);
        }
        if (node["Limit"]) {
            auto limit = node["Limit"].as<int>();
            rhs.limit(limit);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::options::find> {
    using FindOptions = mongocxx::options::find;
    static Node encode(const FindOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, FindOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["Hint"]) {
            auto h = node["Hint"].as<std::string>();
            auto hint = mongocxx::hint(h);
            rhs.hint(hint);
        }
        if (node["Limit"]) {
            auto limit = node["Limit"].as<int>();
            rhs.limit(limit);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return true;
    }
};


template <>
struct convert<mongocxx::options::estimated_document_count> {
    using CountOptions = mongocxx::options::estimated_document_count;
    static Node encode(const CountOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, CountOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].as<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return true;
    }
};


template <>
struct convert<mongocxx::options::insert> {
    static Node encode(const mongocxx::options::insert& rhs) {
        return {};
    }
    static bool decode(const Node& node, mongocxx::options::insert& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["Ordered"]) {
            rhs.ordered(node["Ordered"].as<bool>());
        }
        if (node["BypassDocumentValidation"]) {
            rhs.bypass_document_validation(node["BypassDocumentValidation"].as<bool>());
        }
        if (node["WriteConcern"]) {
            rhs.write_concern(node["WriteConcern"].as<mongocxx::write_concern>());
        }

        return true;
    }
};

template <>
struct convert<mongocxx::options::transaction> {
    using TransactionOptions = mongocxx::options::transaction;
    static Node encode(const TransactionOptions& rhs) {
        return {};
    }

    static bool decode(const Node& node, TransactionOptions& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].as<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        if (node["ReadConcern"]) {
            auto rc = node["ReadConcern"].as<mongocxx::read_concern>();
            rhs.read_concern(rc);
        }
        if (node["ReadPreference"]) {
            auto rp = node["ReadPreference"].as<mongocxx::read_preference>();
            rhs.read_preference(rp);
        }
        return true;
    }
};
}  // namespace YAML

#endif  // HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED
