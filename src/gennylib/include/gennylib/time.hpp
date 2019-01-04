#ifndef HEADER_6540F44B_FA9D_4EBB_904C_E13B7068F478_INCLUDED
#define HEADER_6540F44B_FA9D_4EBB_904C_E13B7068F478_INCLUDED

#include <chrono>
#include <cstdint>

namespace genny::time {
using Duration = std::chrono::microseconds;

struct Keys {
    static constexpr auto kMicroseconds = "us";
    static constexpr auto kMilliseconds = "ms";
    static constexpr auto kSeconds = "s";
};

template<typename T>
inline constexpr int64_t ticks(const Duration & d) {
    return std::chrono::duration_cast<T>(d).count();
}

inline constexpr int64_t micros(const Duration & d){
    return ticks<std::chrono::microseconds>(d);
}

inline constexpr int64_t millis(const Duration & d){
    return ticks<std::chrono::milliseconds>(d);
}

inline constexpr int64_t seconds(const Duration & d){
    return ticks<std::chrono::seconds>(d);
}

} // namespace genny::time

namespace YAML {

template <>
struct convert<genny::time::Duration> {
    using Type = genny::time::Duration;
    using Keys = genny::time::Keys;

    static Node encode(const Type& rhs) {
        Node node;

        node["Ticks"] = rhs.count();
        node["Unit"] = Keys::kMicroseconds;

        return node;
    }

    template<typename T>
    static genny::time::Duration toDuration(const Node & node){
        auto ticks = node.as<typename T::rep>();
        return std::chrono::duration_cast<genny::time::Duration>(T{ticks});
    }

    static bool decode(const Node& node, Type& rhs) {
        // Default to treating a scalar value as a Duration in milliseconds
        if (node.IsScalar()) {
            rhs = toDuration<std::chrono::milliseconds>(node);
            return true;
        }

        // Check a map against our known units
        if(node.IsMap()){
            auto yamlUnit = node["Unit"];
            auto yamlTicks = node["Ticks"];

            if(!yamlUnit || !yamlTicks)
                return false;

            auto unit = yamlUnit.as<std::string>();
            if(unit == Keys::kMicroseconds){
                rhs = toDuration<std::chrono::microseconds>(yamlTicks);
            }else if (unit == Keys::kMilliseconds) {
                rhs = toDuration<std::chrono::milliseconds>(yamlTicks);
            } else if (unit == Keys::kSeconds) {
                rhs = toDuration<std::chrono::seconds>(yamlTicks);
            } else {
                // We found a unit we didn't understand
                return false;
            }

            return true;
        }

        return false;
    }
};

} // namespace YAML

#endif // HEADER_6540F44B_FA9D_4EBB_904C_E13B7068F478_INCLUDED
