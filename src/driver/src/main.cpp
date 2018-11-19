#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>

#include <yaml-cpp/yaml.h>

#include <gennylib/metrics.hpp>
#include <gennylib/version.hpp>

#include "DefaultDriver.hpp"


int main(int argc, char** argv) {
    // basically just a test that we're using c++17
    auto v = std::make_optional(genny::getVersion());

    std::cout << u8"🧞 Genny" << " Version " << v.value_or("ERROR") << u8" 💝🐹🌇⛔" << std::endl;

    auto opts = genny::driver::ProgramOptions(argc, argv);
    if (opts.isHelp) {
        std::cout << opts.description << std::endl;
        return 0;
    }

    // do this based on -v flag or something
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

    genny::driver::DefaultDriver d;

    return d.run(opts);
}
