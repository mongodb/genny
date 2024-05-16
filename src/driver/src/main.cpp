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

#include <fstream>
#include <iostream>
#include <optional>

#include <gennylib/version.hpp>

#include <driver/v1/DefaultDriver.hpp>


int main(int argc, char** argv) {
    auto opts = genny::driver::DefaultDriver::ProgramOptions(argc, argv);
    if (opts.runMode == genny::driver::DefaultDriver::RunMode::kHelp) {
        auto v = std::make_optional(genny::getVersion());
        // basically just a test that we're using c++17
        std::cout << u8"\n🧞 Genny" << " Version " << v.value_or("ERROR") << u8" 💝🐹🌇⛔\n";
        std::cout << opts.description << std::endl;
        return static_cast<int>(opts.parseOutcome);
    }

    genny::driver::DefaultDriver d;

    auto code = d.run(opts);

    if (code != genny::driver::DefaultDriver::OutcomeCode::kSuccess) {
        std::cout << "                       MAMMA MIA!" << std::endl
                  << "                       \033[31;41m----------\033[m" << std::endl
                  << "                     \033[31;41m_/   \033[1;37;41m|\\/|\033[31;41m   \\_\033[m" << std::endl
                  << "                    \033[31;41m/______________\\\033[m" << std::endl
                  << "                   \033[33m|                |\033[m" << std::endl
                  << "               \033[33m_n  |     \033[mn\033[33m    \033[mn\033[33m     |  n_\033[m" << std::endl
                  << "              \033[33m(_ \\ |     \033[mU\033[33m    \033[mU\033[33m     | / _)\033[m" << std::endl
                  << "                \033[33m\\ \\ \\              / / /\033[m" << std::endl
                  << "                 \033[33m\\ \\ \\\\__________// / /\033[m" << std::endl
                  << "                  \033[33m\\_\033[m|\033[31m-\033[34m║║\033[31m--------\033[34m║║\033[31m-\033[m|\033[33m_/\033[m" << std::endl
                  << "                    |\033[31m-\033[m___\033[31m------\033[m___\033[31m-\033[m|" << std::endl
                  << "                   \033[33m_\033[m|/\033[34m###\033[m\\====/\033[34m###\033[m\\|\033[33m_\033[m" << std::endl
                  << "                  \033[33m/..\\\033[34m###\033[m||\033[34m##\033[m||\033[34m###\033[33m/..\\\033[m" << std::endl
                  << "                 \033[33m|....|\033[34m##\033[m//\033[34m##\033[m\\\\\033[34m##\033[33m|....|\033[m" << std::endl
                  << "                 \033[33m|....|\033[34m#\033[m//\033[34m####\033[m\\\\\033[34m#\033[33m|....|\033[m" << std::endl
                  << "                  \033[33m\\___/\033[m/¯¯¯¯¯¯¯¯\\\033[33m\\___/\033[m" << std::endl;
    }

    return static_cast<int>(code);
}
