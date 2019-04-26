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

#ifndef HEADER_E501BBB0_810A_4185_96B2_60CE322C4B78_INCLUDED
#define HEADER_E501BBB0_810A_4185_96B2_60CE322C4B78_INCLUDED

#include <catch2/catch.hpp>

#include <string>

#include <boost/regex.hpp>

#include <bsoncxx/document/view_or_value.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <yaml-cpp/yaml.h>

namespace genny {

inline std::string toString(const std::string& str) {
    return str;
}

inline std::string toString(const bsoncxx::document::view_or_value& t) {
    return bsoncxx::to_json(t, bsoncxx::ExtendedJsonMode::k_canonical);
}

inline std::string toString(const YAML::Node& node) {
    YAML::Emitter out;
    out << node;
    return std::string{out.c_str()};
}

inline std::string toString(int i) {
    return std::to_string(i);
}

/**
 * @tparam Client either mongocxx::client or *mongocxx::pool::entry
 */
template <typename Client>
inline void dropAllDatabases(Client& client) {
    for (auto&& dbDoc : client.list_databases()) {
        const auto dbName = dbDoc["name"].get_utf8().value;
        const auto dbNameString = std::string(dbName);
        if (dbNameString != "admin" && dbNameString != "config" && dbNameString != "local") {
            client.database(dbName).drop();
        }
    }
}

// The matcher class
class MultiLineRegexMatch : public Catch::MatcherBase<std::string> {
    std::string regex;

public:
    explicit MultiLineRegexMatch(std::string regex) : regex{regex} {}

    // Performs the test for this matcher
    virtual bool match(const std::string& matchee) const override {
        try {
            boost::regex reg(
                regex, boost::regex::ECMAScript | boost::regex::newline_alt | boost::regex::icase);
            auto out = boost::regex_match(matchee, reg);
            return out;
        } catch (const std::exception& x) {
            FAIL("Invalid regex: " << x.what());
            return false;
        }
    }

    // Produces a string describing what this matcher does. It should
    // include any provided data (the begin/ end in this case) and
    // be written as if it were stating a fact (in the output it will be
    // preceded by the value under test).
    virtual std::string describe() const override {
        std::ostringstream ss;
        ss << "matches ECMAScript,newline,icase regex /" << regex << "/";
        return ss.str();
    }
};

// The builder function
inline MultiLineRegexMatch MultilineMatch(std::string regex) {
    return MultiLineRegexMatch{regex};
}

}  // namespace genny

#endif  // HEADER_E501BBB0_810A_4185_96B2_60CE322C4B78_INCLUDED
