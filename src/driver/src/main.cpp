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
        return 0;
    }

    genny::driver::DefaultDriver d;

    auto code = d.run(opts);
    return static_cast<int>(code);
}
