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
 */
struct Integer {
    Integer() = default;
    ~Integer() = default;

    explicit Integer(int64_t v) : value(v) {}
    int64_t value;

    explicit operator int64_t() {
        return value;
    }

    explicit operator int32_t() {
        return static_cast<int32_t>(value);
    }

    explicit operator uint() {
        return static_cast<uint>(value);
    }
};

/**
 * Intermediate state for converting YAML syntax into a native std::chrono::duration type of your
 * choice.
 */
struct Time {
    Time() = default;
    ~Time() = default;

    explicit Time(std::chrono::nanoseconds v) : value(v) {}

    // Allow construction with integers for testing.
    explicit Time(int64_t v) : value(v) {}

    std::chrono::nanoseconds value;  // Use the highest precision internally.

    explicit operator std::chrono::nanoseconds() {
        return value;
    }

    explicit operator std::chrono::microseconds() {
        return std::chrono::duration_cast<std::chrono::microseconds>(value);
    }

    explicit operator std::chrono::milliseconds() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(value);
    }

    explicit operator std::chrono::seconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(value);
    }

    explicit operator std::chrono::minutes() {
        return std::chrono::duration_cast<std::chrono::minutes>(value);
    }

    explicit operator std::chrono::hours() {
        return std::chrono::duration_cast<std::chrono::hours>(value);
    }

    int64_t count() const {
        return value.count();
    }
};

/**
 * Rate defined as X operations per Y duration.
 */
struct Rate {
    Rate() = default;
    ~Rate() = default;

    Rate(Time t, Integer i) : per(t), operations(i) {}

    // Allow construction with integers for testing.
    Rate(int64_t t, int64_t i) : per(t), operations(i) {}
    Time per;
    Integer operations;
};

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
struct convert<genny::Rate> {
    static Node encode(const genny::Rate& rhs) {
        std::stringstream msg;
        msg << rhs.operations.value << " per " << rhs.per.count() << " nanoseconds";
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::Rate& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        auto strRepr = node.as<std::string>();

        // Use space as the delimiter.
        auto spacePos = strRepr.find(" per ");

        if (spacePos == std::string::npos) {
            std::stringstream msg;
            msg << "Invalid value for genny::Time field, expected a space separated integer and "
                   "time unit. Saw: "
                << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        auto opCountYaml = Load(strRepr.substr(0, spacePos));
        auto opCount = opCountYaml.as<genny::Integer>();

        auto timeUnitYaml = Load(strRepr.substr(spacePos + 5));
        auto timeUnit = timeUnitYaml.as<genny::Time>();

        rhs = genny::Rate(timeUnit, opCount);

        return true;
    }
};

/**
 * Convert between YAML and genny::Integer
 *
 * The YAML syntax accepts regular and scientific notation decimal values.
 */
template <>
struct convert<genny::Integer> {
    static Node encode(const genny::Integer& rhs) {
        return Node{rhs.value};
    }

    static bool decode(const Node& node, genny::Integer& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }

        auto strRepr = node.as<std::string>();
        size_t pos = 0;

        // Use double here to support the scientific notation.
        double_t num = std::stod(strRepr, &pos);

        if (pos != strRepr.length() || std::llround(num) != num) {
            std::stringstream msg;
            msg << "Invalid value for genny::Integer field: " << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        rhs = genny::Integer(std::llround(num));

        return true;
    }
};

/**
 * Convert between YAML and genny::Time.
 *
 * The YAML syntax looks like [genny::Integer] [milliseconds/microseconds/etc...]
 */
template <>
struct convert<genny::Time> {
    static Node encode(const genny::Time& rhs) {
        std::stringstream msg;
        msg << rhs.value.count() << " nanoseconds";
        return Node{msg.str()};
    }

    static bool decode(const Node& node, genny::Time& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }

        auto strRepr = node.as<std::string>();

        // Use space as the delimiter.
        auto spacePos = strRepr.find_first_of(' ');

        if (spacePos == std::string::npos) {
            std::stringstream msg;
            msg << "Invalid value for genny::Time field, expected a space separated integer and "
                   "time unit. Saw: "
                << strRepr;
            throw genny::InvalidConfigurationException(msg.str());
        }

        auto timeCountYaml = Load(strRepr.substr(0, spacePos));
        auto timeCount = timeCountYaml.as<genny::Integer>().value;

        auto timeUnit = strRepr.substr(spacePos + 1);

        if (timeUnit.find("nanosecond") != std::string::npos) {
            rhs = genny::Time{std::chrono::nanoseconds(timeCount)};
        } else if (timeUnit.find("microsecond") == 0) {
            rhs = genny::Time{std::chrono::microseconds(timeCount)};
        } else if (timeUnit.find("millisecond") == 0) {
            rhs = genny::Time{std::chrono::milliseconds(timeCount)};
        } else if (timeUnit.find("second") == 0) {
            rhs = genny::Time{std::chrono::seconds(timeCount)};
        } else if (timeUnit.find("minute") == 0) {
            rhs = genny::Time{std::chrono::minutes(timeCount)};
        } else if (timeUnit.find("hour") == 0) {
            rhs = genny::Time{std::chrono::hours(timeCount)};
        } else {
            std::stringstream msg;
            msg << "Invalid unit for genny::Time field: " << timeUnit;
            throw genny::InvalidConfigurationException(msg.str());
        }

        return true;
    }
};

}  // namespace YAML


#endif  // HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
