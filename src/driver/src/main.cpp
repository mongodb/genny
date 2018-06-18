#include <iostream>
#include <optional>

#include <gennylib/metrics.hpp>
#include <gennylib/version.hpp>

#include "DefaultDriver.hpp"


int main(int argc, char** argv) {
    // basically just a test that we're using c++17
    auto v = std::make_optional(genny::getVersion());
    std::cout << u8"🧞 Genny" << " Version " << v.value_or("ERROR") << u8" 💝🐹🌇⛔" << std::endl;

    genny::driver::DefaultDriver d;
    return d.run(argc, argv);
}
