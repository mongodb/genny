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

#ifndef HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
#define HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED

#include <chrono>
#include <climits>
#include <cmath>
#include <sstream>

#include <mongocxx/read_concern.hpp>
#include <mongocxx/read_preference.hpp>
#include <mongocxx/write_concern.hpp>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/Node.hpp>
#include <gennylib/Orchestrator.hpp>

namespace genny {

/**
 * This function converts a YAML::Node into a given type and uses a given fallback.
 * It simplifies a common pattern where you have a member variable that needs to be assigned either
 * the value in a node or a fallback value (traditionally, this involves at least a decltype).
 */
template <typename T, typename S>
void decodeNodeInto(T& out, const genny::Node& node, const S& fallback) {
    out = node.maybe<T>().value_or(fallback);
}

/**
 * Intermediate state for converting YAML syntax into a native integer type of your choice.
 *
 * int64_t is used by default; smaller types can be explicitly converted to as needed.
 */
struct IntegerSpec {
    IntegerSpec() = default;
    ~IntegerSpec() = default;

    IntegerSpec(int64_t v) : value{v} {}
    // int64_t is used by default, you can explicitly cast to another type if needed.
    int64_t value;

    operator int64_t() const {  // NOLINT(google-explicit-constructor)
        return value;
    }
};

inline bool operator==(const IntegerSpec& lhs, const IntegerSpec& rhs) {
    return lhs.value == rhs.value;
}

/**
 * Intermediate state for converting YAML syntax into a native std::chrono::duration type of your
 * choice.
 */
struct TimeSpec {
public:  // Typedefs/Structs
    // Use the highest precision internally.
    using ValueT = std::chrono::nanoseconds;

public:  // Ctors/Dtors/Generators
    ~TimeSpec() = default;

    explicit constexpr TimeSpec(ValueT v) : value{v} {}

    // Allow construction with integers for testing.
    explicit constexpr TimeSpec(int64_t v) : TimeSpec{ValueT(v)} {}

    // Default construct as a zero value
    constexpr TimeSpec() : TimeSpec(ValueT::zero()) {}

public:  // Members
    ValueT value;

public:  // Operators
    constexpr operator ValueT() const {
        return value;
    }

    constexpr auto count() const {
        return value.count();
    }

    explicit constexpr operator std::chrono::seconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(value);
    }

    explicit constexpr operator std::chrono::milliseconds() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(value);
    }

    explicit constexpr operator bool() const {
        return count() != 0;
    }
};

inline bool operator==(const TimeSpec& lhs, const TimeSpec& rhs) {
    return lhs.value == rhs.value;
}

// Use the underlying type in TimeSpec as the default Duration type
using Duration = typename TimeSpec::ValueT;

/**
 * BaseRateSpec defined as X operations per Y duration.
 */
struct BaseRateSpec {
    BaseRateSpec() = default;
    ~BaseRateSpec() = default;

    BaseRateSpec(TimeSpec t, IntegerSpec i) : per{t.count()}, operations{i.value} {}

    // Allow construction with integers for testing.
    BaseRateSpec(int64_t t, int64_t i) : per{t}, operations{i} {}
    std::chrono::nanoseconds per;
    int64_t operations;
};

inline bool operator==(const BaseRateSpec& lhs, const BaseRateSpec& rhs) {
    return (lhs.per == rhs.per) && (lhs.operations == rhs.operations);
}

/**
 * PercentileRateSpec defined as X% of max throughput, where X is a whole number.
 */
struct PercentileRateSpec {
    PercentileRateSpec() = default;
    ~PercentileRateSpec() = default;

    PercentileRateSpec(IntegerSpec i) : percent{i.value} {}

    // Allow construction with integers for testing.
    PercentileRateSpec(int64_t i) : percent{i} {}
    int64_t percent;
};

inline bool operator==(const PercentileRateSpec& lhs, const PercentileRateSpec& rhs) {
    return (lhs.percent == rhs.percent);
}

/**
 * RateSpec defined as either X operations per Y duration or Z% of max throughput each phase.
 */
class RateSpec {
public:
    RateSpec() = default;
    ~RateSpec() = default;

    RateSpec(BaseRateSpec s) : _spec{s} {}

    RateSpec(PercentileRateSpec s) : _spec{s} {}

    std::optional<BaseRateSpec> getBaseSpec() const {
        if (auto pval = std::get_if<BaseRateSpec>(&_spec)) {
            return *pval;
        } else {
            return std::nullopt;
        }
    }

    std::optional<PercentileRateSpec> getPercentileSpec() const {
        if (auto pval = std::get_if<PercentileRateSpec>(&_spec)) {
                return *pval;
        } else {
            return std::nullopt;
        }
    }

    inline bool operator==(const RateSpec& rhs) {
        // Equality is well-behaved for variants if it is for their contents.
        return _spec == rhs._spec;
    }

private:
    std::variant<std::monostate, BaseRateSpec, PercentileRateSpec> _spec;
};


struct PhaseRangeSpec {
    PhaseRangeSpec() = default;
    ~PhaseRangeSpec() = default;

    PhaseRangeSpec(genny::IntegerSpec s, genny::IntegerSpec e)
        : start{static_cast<genny::PhaseNumber>(s.value)},
          end{static_cast<genny::PhaseNumber>(e.value)} {
        if (!(s.value >= 0 && s.value <= UINT_MAX)) {
            std::stringstream msg;
            msg << "Invalid start value for genny::PhaseRangeSpec: '" << s.value << "'."
                << " The value must be of type 'u_int32_t";
            throw genny::InvalidConfigurationException(msg.str());
        }
        if (!(e.value >= 0 && e.value <= UINT_MAX)) {
            std::stringstream msg;
            msg << "Invalid end value for genny::PhaseRangeSpec: '" << e.value << "'."
                << " The value must be of type 'u_int32_t";
            throw genny::InvalidConfigurationException(msg.str());
        }
    }
    PhaseRangeSpec(genny::IntegerSpec s) : PhaseRangeSpec(s, s) {}

    genny::PhaseNumber start;
    genny::PhaseNumber end;
};

}  // namespace genny

namespace YAML {

using genny::decodeNodeInto;

template <>
struct convert<mongocxx::read_preference> {
    using ReadPreference = mongocxx::read_preference;
    using ReadMode = mongocxx::read_preference::read_mode;
    static Node encode(const ReadPreference& rhs) {
        Node node;
        auto mode = rhs.mode();
        if (mode == ReadMode::k_primary) {
            node["ReadMode"] = "primary";
        } else if (mode == ReadMode::k_primary_preferred) {
            node["ReadMode"] = "primaryPreferred";
        } else if (mode == ReadMode::k_secondary) {
            node["ReadMode"] = "secondary";
        } else if (mode == ReadMode::k_secondary_preferred) {
            node["ReadMode"] = "secondaryPreferred";
        } else if (mode == ReadMode::k_nearest) {
            node["ReadMode"] = "nearest";
        }
        auto maxStaleness = rhs.max_staleness();
        if (maxStaleness) {
            node["MaxStaleness"] = genny::TimeSpec(*maxStaleness);
        }
        return node;
    }

    static bool decode(const Node& node, ReadPreference& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (!node["ReadMode"]) {
            // readPreference must have a read mode specified.
            return false;
        }
        auto readMode = node["ReadMode"].as<std::string>();
        if (readMode == "primary") {
            rhs.mode(ReadMode::k_primary);
        } else if (readMode == "primaryPreferred") {
            rhs.mode(ReadMode::k_primary_preferred);
        } else if (readMode == "secondary") {
            rhs.mode(ReadMode::k_secondary);
        } else if (readMode == "secondaryPreferred") {
            rhs.mode(ReadMode::k_secondary_preferred);
        } else if (readMode == "nearest") {
            rhs.mode(ReadMode::k_nearest);
        } else {
            return false;
        }
        if (node["MaxStaleness"]) {
            auto maxStaleness = node["MaxStaleness"].as<genny::TimeSpec>();
            rhs.max_staleness(std::chrono::seconds{maxStaleness});
        }
        return true;
    }
};

template <>
struct convert<mongocxx::write_concern> {
    using WriteConcern = mongocxx::write_concern;
    static Node encode(const WriteConcern& rhs) {
        Node node;
        node["Timeout"] = genny::TimeSpec{rhs.timeout()};

        auto journal = rhs.journal();
        node["Journal"] = journal;

        auto ackLevel = rhs.acknowledge_level();
        if (ackLevel == WriteConcern::level::k_majority) {
            node["Level"] = "majority";
        } else if (ackLevel == WriteConcern::level::k_acknowledged) {
            auto numNodes = rhs.nodes();
            if (numNodes) {
                node["Level"] = *numNodes;
            }
        }
        return node;
    }

    static bool decode(const Node& node, WriteConcern& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (!node["Level"]) {
            // writeConcern must specify the write concern level.
            return false;
        }
        auto level = node["Level"].as<std::string>();
        try {
            auto level = node["Level"].as<int>();
            rhs.nodes(level);
        } catch (const BadConversion& e) {
            auto level = node["Level"].as<std::string>();
            if (level == "majority") {
                rhs.majority(std::chrono::milliseconds{0});
            } else {
                // writeConcern level must be of valid integer or 'majority'.
                return false;
            }
        }
        if (node["Timeout"]) {
            auto timeout = node["Timeout"].as<genny::TimeSpec>();
            rhs.timeout(std::chrono::milliseconds{timeout});
        }
        if (node["Journal"]) {
            auto journal = node["Journal"].as<bool>();
            rhs.journal(journal);
        }
        return true;
    }
};

template <>
struct convert<mongocxx::read_concern> {
    using ReadConcern = mongocxx::read_concern;
    using ReadConcernLevel = mongocxx::read_concern::level;
    static Node encode(const ReadConcern& rhs) {
        Node node;
        auto level = std::string(rhs.acknowledge_string());
        if (!level.empty()) {
            node["Level"] = level;
        }
        return node;
    }

    static bool isValidReadConcernString(std::string_view rcString) {
        return (rcString == "local" || rcString == "majority" || rcString == "linearizable" ||
                rcString == "snapshot" || rcString == "available");
    }

    static bool decode(const Node& node, ReadConcern& rhs) {
        if (!node.IsMap()) {
            return false;
        }
        if (!node["Level"]) {
            // readConcern must have a read concern level specified.
            return false;
        }
        auto level = node["Level"].as<std::string>();
        if (isValidReadConcernString(level)) {
            rhs.acknowledge_string(level);
            return true;
        } else {
            return false;
        }
    }
};

/**
 * Convert between YAML and genny::PhaseRange
 *
 * The YAML syntax accepts "[genny::Integer]..[genny::Integer]".
 * This is used to stipulate repeating a phase N number of times
 */
template <>
struct convert<genny::PhaseRangeSpec> {
    static Node encode(const genny::PhaseRangeSpec rhs) {
        std::stringstream msg;
        msg << rhs.start << ".." << rhs.end;
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::PhaseRangeSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        auto strRepr = node.as<std::string>();

        // use '..' as delimiter.
        constexpr std::string_view delimiter = "..";
        auto delimPos = strRepr.find(delimiter);

        if (delimPos == std::string::npos) {
            // check if user inputted an integer
            try {
                auto phaseNumberYaml = node.as<genny::IntegerSpec>();
                rhs = genny::PhaseRangeSpec(phaseNumberYaml);
                return true;
            } catch (const genny::InvalidConfigurationException& e) {
                std::stringstream msg;
                msg << "Invalid value for genny::PhaseRangeSpec: '" << strRepr << "'."
                    << " The correct syntax is either a single integer or two integers delimited "
                       "by '..'";
                throw genny::InvalidConfigurationException(msg.str());
            }
        }

        genny::IntegerSpec start;
        genny::IntegerSpec end;

        auto startYaml = Load(strRepr.substr(0, delimPos));
        auto endYaml = Load(strRepr.substr(delimPos + delimiter.size()));

        try {
            start = startYaml.as<genny::IntegerSpec>();
            end = endYaml.as<genny::IntegerSpec>();
        } catch (const genny::InvalidConfigurationException& e) {
            std::stringstream msg;
            msg << "Invalid value for genny::PhaseRangeSpec: '" << strRepr << "'."
                << " The correct syntax is two integers delimited by '..'";
            throw genny::InvalidConfigurationException(msg.str());
        }

        if (start > end) {
            std::stringstream msg;
            msg << "Invalid value for genny::PhaseRangeSpec: '" << strRepr << "'."
                << " The start value cannot be greater than the end value.";
            throw genny::InvalidConfigurationException(msg.str());
        }
        rhs = genny::PhaseRangeSpec(start, end);

        return true;
    }
};

/**
 * Convert between YAML and genny::BaseRateSpec
 *
 * The YAML syntax accepts [genny::Integer] per [genny::Time]
 * The syntax is interpreted as operations per unit of time.
 */
template <>
struct convert<genny::BaseRateSpec> {
    static Node encode(const genny::BaseRateSpec& rhs) {
        std::stringstream msg;
        msg << rhs.operations << " per " << rhs.per.count() << " nanoseconds";
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::BaseRateSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        auto strRepr = node.as<std::string>();

        // Use space as the delimiter.
        const std::string delimiter = " per ";
        auto spacePos = strRepr.find(delimiter);

        if (spacePos == std::string::npos) {
            std::stringstream msg;
            msg << "Invalid value for genny::TimeSpec field, expected a space separated integer "
                   "and "
                   "time unit. Saw: "
                << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        auto opCountYaml = Load(strRepr.substr(0, spacePos));
        auto opCount = opCountYaml.as<genny::IntegerSpec>();

        auto timeUnitYaml = Load(strRepr.substr(spacePos + delimiter.size()));
        auto timeUnit = timeUnitYaml.as<genny::TimeSpec>();

        rhs = genny::BaseRateSpec(timeUnit, opCount);

        return true;
    }
};

/**
 * Convert between YAML and genny::PercentileRateSpec
 *
 * The YAML syntax accepts [genny::Integer]%
 * The syntax is interpreted as a percentage of the max throughput.
 */
template <>
struct convert<genny::PercentileRateSpec> {
    static Node encode(const genny::PercentileRateSpec& rhs) {
        std::stringstream msg;
        msg << rhs.percent << "%";
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::PercentileRateSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        auto strRepr = node.as<std::string>();

        // Use percent as the delimiter.
        const std::string delimiter = "%";
        auto percentPos = strRepr.find(delimiter);

        if (percentPos == std::string::npos || percentPos != strRepr.size() - 1) {
            std::stringstream msg;
            msg << "Invalid value for PercentileRateSpec field, expected an integer followed by %."
                   " Saw: "
                << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        auto percentYaml = Load(strRepr.substr(0, percentPos));
        auto percent = percentYaml.as<genny::IntegerSpec>();

        if (percent.value > 100) {
            std::stringstream msg;
            msg << "Invalid value for PercentileRateSpec field, integer cannot be greater than 100."
                   " Saw: "
                << percent.value;
            throw genny::InvalidConfigurationException(msg.str());
        }

        rhs = genny::PercentileRateSpec(percent);

        return true;
    }
};

/**
 * Convert between YAML and genny::RateSpec
 *
 * The YAML syntax accepts either [genny::Integer] per [genny::Time]
 * or [genny::Integer]%
 *
 * The syntax is interpreted as operations per unit of time or
 * percentage of max throughput.
 */
template <>
struct convert<genny::RateSpec> {
    static Node encode(const genny::RateSpec& rhs) {
        std::stringstream msg;

        if (auto spec = rhs.getBaseSpec()) {
            msg << spec->operations << " per " << spec->per.count() << " nanoseconds";
        } else if (auto spec = rhs.getPercentileSpec()) {
            msg << spec->percent << "%";
        } else {
            throw genny::InvalidConfigurationException("Cannot encode empty RateSpec.");
        }

        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::RateSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }

        auto strRepr = node.as<std::string>();
        auto nodeYaml = Load(strRepr);

        // First treat as a BaseRateSpec, then try as a PercentileRateSpec.
        try {
            auto baseSpec = nodeYaml.as<genny::BaseRateSpec>();
            rhs = genny::RateSpec(baseSpec);
            return true;
        } catch (genny::InvalidConfigurationException e) {}

        try {
            auto percentileSpec = nodeYaml.as<genny::PercentileRateSpec>();
            rhs = genny::RateSpec(percentileSpec);
            return true;
        } catch (genny::InvalidConfigurationException e) {}

        std::stringstream msg;
        msg << "Invalid value for RateSpec field, expected a space separated integer and time unit,"
            << " or integer followed by %. Saw: " << strRepr;
        throw genny::InvalidConfigurationException(msg.str());
    }
};



/**
 * Convert between YAML and genny::Integer
 *
 * The YAML syntax accepts regular and scientific notation decimal values.
 */
template <>
struct convert<genny::IntegerSpec> {
    static Node encode(const genny::IntegerSpec& rhs) {
        return Node{rhs.value};
    }

    static bool decode(const Node& node, genny::IntegerSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }

        auto strRepr = node.as<std::string>();
        size_t pos = 0;

        // Use double here to support the scientific notation.
        double_t num = std::stod(strRepr, &pos);

        if (pos != strRepr.length() || std::llround(num) != num) {
            std::stringstream msg;
            msg << "Invalid value for genny::IntegerSpec field: " << strRepr
                << " from config: " << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        if (num < 0) {
            std::stringstream msg;
            msg << "Value for genny::IntegerSpec can't be negative: " << num
                << " from config: " << strRepr;
            ;
            throw genny::InvalidConfigurationException(msg.str());
        }

        rhs = genny::IntegerSpec(std::llround(num));

        return true;
    }
};

/**
 * Convert between YAML and genny::Time.
 *
 * The YAML syntax looks like [genny::Integer] [milliseconds/microseconds/etc...]
 */
template <>
struct convert<genny::TimeSpec> {
    static Node encode(const genny::TimeSpec& rhs) {
        std::stringstream msg;
        msg << rhs.value.count() << " nanoseconds";
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::TimeSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }

        auto strRepr = node.as<std::string>();

        // Use space as the delimiter.
        auto spacePos = strRepr.find_first_of(' ');

        if (spacePos == std::string::npos) {
            std::stringstream msg;
            msg << "Invalid value for genny::TimeSpec field, expected a space separated integer "
                   "and "
                   "time unit. Saw: "
                << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        auto timeCountYaml = Load(strRepr.substr(0, spacePos));
        auto timeCount = timeCountYaml.as<genny::IntegerSpec>().value;

        auto timeUnit = strRepr.substr(spacePos + 1);

        // Use string::find here so plurals get parsed correctly.
        if (timeUnit.find("nanosecond") == 0) {
            rhs = genny::TimeSpec{std::chrono::nanoseconds(timeCount)};
        } else if (timeUnit.find("microsecond") == 0) {
            rhs = genny::TimeSpec{std::chrono::microseconds(timeCount)};
        } else if (timeUnit.find("millisecond") == 0) {
            rhs = genny::TimeSpec{std::chrono::milliseconds(timeCount)};
        } else if (timeUnit.find("second") == 0) {
            rhs = genny::TimeSpec{std::chrono::seconds(timeCount)};
        } else if (timeUnit.find("minute") == 0) {
            rhs = genny::TimeSpec{std::chrono::minutes(timeCount)};
        } else if (timeUnit.find("hour") == 0) {
            rhs = genny::TimeSpec{std::chrono::hours(timeCount)};
        } else {
            std::stringstream msg;
            msg << "Invalid unit: " << timeUnit
                << " for genny::TimeSpec field in config: " << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        return true;
    }
};

}  // namespace YAML


#endif  // HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
