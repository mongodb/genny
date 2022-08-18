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

namespace genny::actor {

struct ExternalProgramRunner::PhaseConfig {
    std::string setupFilename;
    std::string programFilename;

    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : setupFilename{phaseContext["Setup"].to<std::string>()},
          programFilename{phaseContext["Run"].to<std::string>()} {}
};

void ExternalProgramRunner::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {

            auto setupPermissionCmd = "chmod u+x " + config->setupFilename;
            const char* setupPermissionCmdPtr = &*setupPermissionCmd.begin();
            system(setupPermissionCmdPtr);

            auto programPermissionCmd = "chmod u+x " + config->programFilename;
            const char* programPermissionCmdPtr = &*programPermissionCmd.begin();
            system(programPermissionCmdPtr);

            BOOST_LOG_TRIVIAL(info) << " ExternalProgramRunner setting up "
                                    << config->setupFilename;

            auto setupCmd = "./" + config->setupFilename + " >stdout-setup.txt";
            const char* setupCmdPtr = &*setupCmd.begin();
            system(setupCmdPtr);

            BOOST_LOG_TRIVIAL(warning) << " ExternalProgramRunner running "
                                    << config->programFilename;

            auto programCommand = "./" + config->programFilename + " >stdout-run.txt";
            const char* programCmdPtr = &*programCommand.begin();
            system(programCmdPtr);
        }
    }
}

ExternalProgramRunner::ExternalProgramRunner(genny::ActorContext& context)
    : Actor{context},
      _loop{context, ExternalProgramRunner::id()} {}

namespace {
auto registerExternalProgramRunner = Cast::registerDefault<ExternalProgramRunner>();
}  // namespace
}  // namespace genny::actor
