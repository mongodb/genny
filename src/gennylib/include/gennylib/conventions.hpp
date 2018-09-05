#ifndef HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
#define HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED

#include <chrono>

#include <yaml-cpp/yaml.h>

namespace YAML {


template <>
struct convert<std::chrono::milliseconds> {
    // For now only accept `Duration: 300` for 300 milliseconds.
    // Future change will parse this as a string e.g. `Duration: 300 milliseconds`.

    static Node encode(const std::chrono::milliseconds& rhs) {
        return Node{rhs.count()};
    }

    static bool decode(const Node& node, std::chrono::milliseconds& rhs) {
        if (node.IsSequence() || node.IsMap()) {
            return false;
        }
        rhs = std::chrono::milliseconds{node.as<int>()};
        return true;
    }
};

}  // namespace YAML


#endif  // HEADER_CC9B7EF0_9FB9_4AD4_B64C_DC7AE48F72A6_INCLUDED
