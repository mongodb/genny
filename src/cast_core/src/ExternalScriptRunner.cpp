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

#include <cast_core/actors/ExternalScriptRunner.hpp>

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
#include <cstdio>
#include <stdexcept>
#include <array>
#include <stdio.h>
#include <unistd.h>

namespace genny::actor {

struct ExternalScriptRunner::PhaseConfig {
    std::string script;

    std::string mongoServerURI;

    // Record data retruned by the script
    metrics::Operation operation;

    // Record data of the script operation itself
    metrics::Operation scriptOperation;

    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : script{phaseContext["Script"].to<std::string>()},
          mongoServerURI{phaseContext["MongoServerURI"].maybe<std::string>().value_or("--nodb")},
          operation{phaseContext.operation("DefaultMetricsName", id)},
          scriptOperation{phaseContext.namedOperation("ExternalScript", id)} {}
};

/**
 * @brief Execute external script
 * 
 * @param cmd 
 * @return std::string 
 */
std::string ExternalScriptRunner::exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void ExternalScriptRunner::run() {
    // This section gets executed before the phases. Setup is executed here, so maybe
    // we could add some sanity checks before actually running the shell.

    for (auto&& config : _loop) {

        std::string programCommand = "";
        
        if (_command == "mongosh") {
            programCommand = "mongosh " + config->mongoServerURI + " --quiet --eval \"" + config->script + "\"";
        }
        else if (_command == "sh") {
            programCommand = "sh -c \"" + config->script + "\"";
        }
        else {
            throw std::runtime_error("Script type " + _command + " is not supported.");
        }
        
        BOOST_LOG_TRIVIAL(info) << "ExternalScriptRunner running "
                                << programCommand;

        for (const auto&& _ : config) {

            // Execute the script and read result from stdout
            auto ctx = config->scriptOperation.start();
            const char* programCmdPtr = &*programCommand.begin();
            std::string result = exec(programCmdPtr);
            ctx.success();

            try {

                // If the result from stdout can be parsed as integer, report the operation metrics
                // Otherwise, ignore the result
                int num = std::stoi(result);
                config->operation.report(metrics::clock::now(),
                                              std::chrono::milliseconds{num}); 
            }
            catch(std::exception& err) {
                BOOST_LOG_TRIVIAL(info) << err.what();
            }
        }
    }
}

ExternalScriptRunner::ExternalScriptRunner(genny::ActorContext& context)
    // These are the attributes for the actor.
    : Actor{context},
      _command{std::move(context.get("Command").maybe<std::string>().value_or(""))},
      _loop{context, ExternalScriptRunner::id()} {}

namespace {
auto registerExternalScriptRunner = Cast::registerDefault<ExternalScriptRunner>();
}  // namespace
}  // namespace genny::actor
