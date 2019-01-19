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

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>

#include <yaml-cpp/yaml.h>

#include <gennylib/version.hpp>

#include <DefaultDriver.hpp>


int main(int argc, char** argv) {
    // basically just a test that we're using c++17
    auto v = std::make_optional(genny::getVersion());

    std::cout << u8"🧞 Genny" << " Version " << v.value_or("ERROR") << u8" 💝🐹🌇⛔" << std::endl;

    auto opts = genny::driver::DefaultDriver::ProgramOptions(argc, argv);
    if (opts.isHelp) {
        std::cout << opts.description << std::endl;
        return 0;
    }

    // do this based on -v flag or something
    boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

    genny::driver::DefaultDriver d;

    auto code = d.run(opts);
    return static_cast<int>(code);
}
