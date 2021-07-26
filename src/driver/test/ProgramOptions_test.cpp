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

#include <testlib/helpers.hpp>

#include <driver/v1/DefaultDriver.hpp>

TEST_CASE("ProgramOptions behavior") {
    SECTION("missing subcommand") {
        const char* argv[] = {"run-genny"};
        auto opts = genny::driver::DefaultDriver::ProgramOptions(1, (char**)argv);
        REQUIRE(opts.parseOutcome == genny::driver::DefaultDriver::OutcomeCode::kUserException);
    }

    SECTION("invalid subcommand") {
        const char* argv[] = {"run-genny", "use-postgresql"};
        auto opts = genny::driver::DefaultDriver::ProgramOptions(2, (char**)argv);
        REQUIRE(opts.parseOutcome == genny::driver::DefaultDriver::OutcomeCode::kUserException);
    }

    SECTION("valid subcommand") {
        const char* argv[] = {"run-genny", "dry-run"};
        auto opts = genny::driver::DefaultDriver::ProgramOptions(2, (char**)argv);
        REQUIRE(opts.parseOutcome == genny::driver::DefaultDriver::OutcomeCode::kSuccess);
    }
}
