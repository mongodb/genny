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

    // These are the attributes of each phase. Here we could add the parameters to the
    // execution of the mongo shell.
    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : programFilename{phaseContext["Run"].to<std::string>()},
          outputFilename{phaseContext["Output"].to<std::string>()} {}
};

void ExternalProgramRunner::run() {
    // This section gets executed before the phases. Setup is executed here, so maybe
    // we could add some sanity checks before actually running the shell.

    BOOST_LOG_TRIVIAL(info) << "ExternalProgramRunner setting up "
                            << _setupCmd;

    // Change permissions of the executable file so that it can run.
    boost::filesystem::permissions(_setupCmd, boost::filesystem::perms::owner_all);

    // Build the command to run the executable file and output it to a text file.
    auto setupCmd = "./" + _setupCmd + " >setup.txt";
    const char* setupCmdPtr = &*setupCmd.begin();

    // Command gets executed. I believe system() returns an int with the process ID so there
    // might be a way to get the stdout from that instead of writing to a file.
    system(setupCmdPtr);

    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            // This is the phase execution. Instead of config->programFilename we can 
            // replace this for mongosh.

            BOOST_LOG_TRIVIAL(info) << "ExternalProgramRunner running "
                                    << config->programFilename;

            // Change permissions of the executable file so that it can run. Since we'll only execute
            // mongsh we can move this permission to the setup area so that it's only ran once.
            boost::filesystem::permissions(config->programFilename, boost::filesystem::perms::owner_all);

            // Build the command to run the executable file and output it to a text file.
            auto programCommand = "./" + config->programFilename + " >" + config->outputFilename;
            const char* programCmdPtr = &*programCommand.begin();

            // Command gets executed.
            system(programCmdPtr);
        }
    }
}

ExternalProgramRunner::ExternalProgramRunner(genny::ActorContext& context)
    // These are the attributes for the actor.
    : Actor{context},
      _setupCmd{std::move(context.get("Setup").maybe<std::string>().value_or(""))},
      _loop{context, ExternalProgramRunner::id()} {}

namespace {
auto registerExternalProgramRunner = Cast::registerDefault<ExternalProgramRunner>();
}  // namespace
}  // namespace genny::actor
