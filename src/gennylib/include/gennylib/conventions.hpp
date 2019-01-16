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
#include <cmath>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include <gennylib/InvalidConfigurationException.hpp>

namespace genny {

/**
 * This function converts a YAML::Node into a given type and uses a given fallback.
 * It simplifies a common pattern where you have a member variable that needs to be assigned either
 * the value in a node or a fallback value (traditionally, this involves at least a decltype).
 */
template <typename T, typename S>
void decodeNodeInto(T& out, const YAML::Node& node, const S& fallback) {
    out = node.as<T>(fallback);
}

/**
 * Intermediate state for converting YAML syntax into a native integer type of your choice.
 *
 * size_t is used by default; smaller types can be explicitly converted to as needed.
 */
struct UIntSpec {
    UIntSpec() = default;
    ~UIntSpec() = default;

    explicit UIntSpec(size_t v) : value{v} {}
    // size_t is used by default, you can explicitly cast to another type if needed.
    size_t value;

    // Explicitly cast to avoid narrowing.
    explicit operator uint32_t() {
        return static_cast<uint32_t>(value);
    }

    operator size_t() {  // NOLINT(google-explicit-constructor)
        return value;
    }
};

inline bool operator==(const UIntSpec& lhs, const UIntSpec& rhs) {
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

    // TODO: TIG-1155 remove these conversion operators if they're not used.
    explicit constexpr operator std::chrono::microseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(value);
    }

    explicit constexpr operator std::chrono::milliseconds() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(value);
    }

    explicit constexpr operator std::chrono::seconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(value);
    }

    explicit constexpr operator std::chrono::minutes() const {
        return std::chrono::duration_cast<std::chrono::minutes>(value);
    }

    explicit constexpr operator std::chrono::hours() const {
        return std::chrono::duration_cast<std::chrono::hours>(value);
    }

    constexpr auto count() const {
        return value.count();
    }
};

inline bool operator==(const TimeSpec& lhs, const TimeSpec& rhs) {
    return lhs.value == rhs.value;
}

// Use the underlying type in TimeSpec as the default Duration type
using Duration = typename TimeSpec::ValueT;

/**
 * Rate defined as X operations per Y duration.
 */
struct RateSpec {
    RateSpec() = default;
    ~RateSpec() = default;

    RateSpec(TimeSpec t, UIntSpec i) : per{t.count()}, operations{i.value} {}

    // Allow construction with integers for testing.
    RateSpec(int64_t t, size_t i) : per{t}, operations{i} {}
    std::chrono::nanoseconds per;
    size_t operations;
};

inline bool operator==(const RateSpec& lhs, const RateSpec& rhs) {
    return (lhs.per == rhs.per) && (lhs.operations == rhs.operations);
}

}  // namespace genny

namespace YAML {

using genny::decodeNodeInto;

/**
 * Convert between YAML and genny::Rate
 *
 * The YAML syntax accepts [genny::Integer] per [genny::Time]
 * The syntax is interpreted as operations per unit of time.
 */
template <>
struct convert<genny::RateSpec> {
    static Node encode(const genny::RateSpec& rhs) {
        std::stringstream msg;
        msg << rhs.operations << " per " << rhs.per.count() << " nanoseconds";
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::RateSpec& rhs) {
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
        auto opCount = opCountYaml.as<genny::UIntSpec>();

        auto timeUnitYaml = Load(strRepr.substr(spacePos + delimiter.size()));
        auto timeUnit = timeUnitYaml.as<genny::TimeSpec>();

        rhs = genny::RateSpec(timeUnit, opCount);

        return true;
    }
};

/**
 * Convert between YAML and genny::Integer
 *
 * The YAML syntax accepts regular and scientific notation decimal values.
 */
template <>
struct convert<genny::UIntSpec> {
    static Node encode(const genny::UIntSpec& rhs) {
        return Node{rhs.value};
    }

    static bool decode(const Node& node, genny::UIntSpec& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }

        auto strRepr = node.as<std::string>();
        size_t pos = 0;

        // Use double here to support the scientific notation.
        double_t num = std::stod(strRepr, &pos);

        if (pos != strRepr.length() || std::llround(num) != num) {
            std::stringstream msg;
            msg << "Invalid value for genny::UIntSpec field: " << strRepr
                << " from config: " << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        if (num < 0) {
            std::stringstream msg;
            msg << "Value for genny::UIntSpec can't be negative: " << num
                << " from config: " << strRepr;
            ;
            throw genny::InvalidConfigurationException(msg.str());
        }

        rhs = genny::UIntSpec(std::llround(num));

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
        auto timeCount = timeCountYaml.as<genny::UIntSpec>().value;

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
