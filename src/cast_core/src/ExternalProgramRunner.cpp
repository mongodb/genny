// Copyright 2022-present MongoDB Inc.
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

#include <cast_core/actors/ExternalProgramRunner.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

#include <iostream>
#include <string>
#include <cstdlib>
#include <boost/filesystem.hpp>

namespace genny::actor {

struct ExternalProgramRunner::PhaseConfig {
    std::string programFilename;
    std::string outputFilename;

    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : programFilename{phaseContext["Run"].to<std::string>()},
          outputFilename{phaseContext["Output"].to<std::string>()} {}
};

void ExternalProgramRunner::run() {
    BOOST_LOG_TRIVIAL(info) << "ExternalProgramRunner setting up "
                            << _setupCmd;

    boost::filesystem::permissions(_setupCmd, boost::filesystem::perms::owner_all);
    auto setupCmd = "./" + _setupCmd + " >setup.txt";
    const char* setupCmdPtr = &*setupCmd.begin();
    system(setupCmdPtr);

    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            BOOST_LOG_TRIVIAL(info) << "ExternalProgramRunner running "
                                    << config->programFilename;

            boost::filesystem::permissions(config->programFilename, boost::filesystem::perms::owner_all);
            auto programCommand = "./" + config->programFilename + " >" + config->outputFilename;
            const char* programCmdPtr = &*programCommand.begin();
            system(programCmdPtr);
        }
    }
}

ExternalProgramRunner::ExternalProgramRunner(genny::ActorContext& context)
    : Actor{context},
      _setupCmd{std::move(context.get("Setup").maybe<std::string>().value_or(""))},
      _loop{context, ExternalProgramRunner::id()} {}

namespace {
auto registerExternalProgramRunner = Cast::registerDefault<ExternalProgramRunner>();
}  // namespace
}  // namespace genny::actor
