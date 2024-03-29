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

#include <boost/throw_exception.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/database.hpp>
#include <string_view>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny {

template <>
struct NodeConvert<mongocxx::options::aggregate> {
    using type = mongocxx::options::aggregate;
    static type convert(const Node& node) {
        type rhs{};

        if (node["AllowDiskUse"]) {
            auto allowDiskUse = node["AllowDiskUse"].to<bool>();
            rhs.allow_disk_use(allowDiskUse);
        }
        if (node["BatchSize"]) {
            auto batchSize = node["BatchSize"].to<int>();
            rhs.batch_size(batchSize);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds(maxTime));
        }
        if (node["ReadPreference"]) {
            auto readPreference = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(readPreference);
        }
        if (node["BypassDocumentValidation"]) {
            auto bypassValidation = node["BypassDocumentValidation"].to<bool>();
            rhs.bypass_document_validation(bypassValidation);
        }
        if (node["Hint"]) {
            auto h = node["Hint"].to<std::string>();
            auto hint = mongocxx::hint(std::move(h));
            rhs.hint(hint);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].to<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::bulk_write> {
    using type = mongocxx::options::bulk_write;

    static type convert(const Node& node) {
        type rhs{};

        if (node["BypassDocumentValidation"]) {
            auto bypassDocValidation = node["BypassDocumentValidation"].to<bool>();
            rhs.bypass_document_validation(bypassDocValidation);
        }
        if (node["Ordered"]) {
            auto isOrdered = node["Ordered"].to<bool>();
            rhs.ordered(isOrdered);
        }
        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].to<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::count> {
    using type = mongocxx::options::count;

    static type convert(const Node& node) {
        type rhs{};

        if (node["Hint"]) {
            auto h = node["Hint"].to<std::string>();
            auto hint = mongocxx::hint(std::move(h));
            rhs.hint(hint);
        }
        if (node["Limit"]) {
            auto limit = node["Limit"].to<int>();
            rhs.limit(limit);
        }
        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::find> {
    using type = mongocxx::options::find;

    static type convert(const Node& node) {
        type rhs{};

        if (const auto& allowDiskUse = node["AllowDiskUse"]) {
            rhs.allow_disk_use(allowDiskUse.to<bool>());
        }
        if (const auto& sort = node["Sort"]) {
            rhs.sort(bsoncxx::from_json(sort.to<std::string>()));
        }
        if (const auto& collation = node["Collation"]) {
            rhs.collation(bsoncxx::from_json(collation.to<std::string>()));
        }
        // Note that the conversion of hints (here and elsewhere in this file) could be extended
        // to support the hint specified as a document. Right now it only supports hints specified
        // as a string giving the index name.
        if (const auto& hint = node["Hint"]) {
            rhs.hint(mongocxx::hint(hint.to<std::string>()));
        }
        if (const auto& comment = node["Comment"]) {
            rhs.comment(comment.to<std::string>());
        }
        if (const auto& limit = node["Limit"]) {
            rhs.limit(limit.to<int64_t>());
        }
        if (const auto& skip = node["Skip"]) {
            rhs.skip(skip.to<int64_t>());
        }
        if (const auto& batchSize = node["BatchSize"]) {
            rhs.batch_size(batchSize.to<int32_t>());
        }
        if (const auto& maxTime = node["MaxTime"]) {
            auto max = maxTime.to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{max});
        }
        if (const auto& readPref = node["ReadPreference"]) {
            rhs.read_preference(readPref.to<mongocxx::read_preference>());
        }

        auto getBoolValue = [&](const std::string& paramName) {
            const auto& val = node[paramName];
            return val && val.to<bool>();
        };

        // Figure out the cursor type.
        const bool tailable = getBoolValue("Tailable");
        const bool awaitData = getBoolValue("AwaitData");
        if (tailable && awaitData) {
            rhs.cursor_type(mongocxx::cursor::type::k_tailable_await);
        } else if (tailable) {
            rhs.cursor_type(mongocxx::cursor::type::k_tailable);
        } else if (awaitData) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "Cannot set 'awaitData' to true without also setting 'tailable' to true"));
        } else {
            rhs.cursor_type(mongocxx::cursor::type::k_non_tailable);
        }
        return rhs;
    }
};


template <>
struct NodeConvert<mongocxx::options::estimated_document_count> {
    using type = mongocxx::options::estimated_document_count;

    static type convert(const Node& node) {
        type rhs{};

        if (node["MaxTime"]) {
            auto maxTime = node["MaxTime"].to<genny::TimeSpec>();
            rhs.max_time(std::chrono::milliseconds{maxTime});
        }
        if (node["ReadPreference"]) {
            auto readPref = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(readPref);
        }
        return rhs;
    }
};


template <>
struct NodeConvert<mongocxx::options::insert> {
    using type = mongocxx::options::insert;

    static type convert(const Node& node) {
        type rhs{};

        if (node["Ordered"]) {
            rhs.ordered(node["Ordered"].to<bool>());
        }
        if (node["BypassDocumentValidation"]) {
            rhs.bypass_document_validation(node["BypassDocumentValidation"].to<bool>());
        }
        if (node["WriteConcern"]) {
            rhs.write_concern(node["WriteConcern"].to<mongocxx::write_concern>());
        }

        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::transaction> {
    using type = mongocxx::options::transaction;

    static type convert(const Node& node) {
        type rhs{};

        if (node["WriteConcern"]) {
            auto wc = node["WriteConcern"].to<mongocxx::write_concern>();
            rhs.write_concern(wc);
        }
        if (node["ReadConcern"]) {
            auto rc = node["ReadConcern"].to<mongocxx::read_concern>();
            rhs.read_concern(rc);
        }
        if (node["MaxCommitTime"]) {
            auto maxCommitTime = node["MaxCommitTime"].to<genny::TimeSpec>();
            rhs.max_commit_time_ms(std::chrono::milliseconds(maxCommitTime));
        }
        if (node["ReadPreference"]) {
            auto rp = node["ReadPreference"].to<mongocxx::read_preference>();
            rhs.read_preference(rp);
        }
        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::update> {
    using type = mongocxx::options::update;

    static type convert(const Node& node) {
        type rhs{};

        if (const auto& bypass = node["Bypass"]; bypass) {
            rhs.bypass_document_validation(bypass.to<bool>());
        }
        if (const auto& hint = node["Hint"]; hint) {
            rhs.hint(mongocxx::hint(std::move(hint.to<std::string>())));
        }
        if (const auto& upsert = node["Upsert"]; upsert) {
            rhs.upsert(upsert.to<bool>());
        }
        if (const auto& wc = node["WriteConcern"]; wc) {
            rhs.write_concern(wc.to<mongocxx::write_concern>());
        }

        return rhs;
    }
};

template <>
struct NodeConvert<mongocxx::options::delete_options> {
    using type = mongocxx::options::delete_options;

    static type convert(const Node& node) {
        type rhs{};

        if (const auto& hint = node["Hint"]; hint) {
            rhs.hint(mongocxx::hint(std::move(hint.to<std::string>())));
        }
        if (const auto& wc = node["WriteConcern"]; wc) {
            rhs.write_concern(wc.to<mongocxx::write_concern>());
        }

        return rhs;
    }
};

}  // namespace genny
#endif  // HEADER_1AFC7FF3_F491_452B_9805_18CAEDE4663D_INCLUDED
