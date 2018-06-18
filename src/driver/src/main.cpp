#include <iostream>
#include <optional>
#include <string>

#include <gennylib/version.hpp>

int main() {
    // basically just a test that we're using c++17
    auto v { std::make_optional(genny::getVersion()) };
    std::cout << u8"🧞 Genny" << " Version " << v.value_or("ERROR") << u8" 💝🐹🌇⛔" << std::endl;
    return 0;
}
