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

namespace genny::actor {

struct ExternalScriptRunner::PhaseConfig {
    std::string script;

    std::string mongoServerURI;

    std::string command;

    std::string programCommand;

    // Record data retruned by the script
    metrics::Operation operation;

    // Record data of the script operation itself
    metrics::Operation scriptOperation;

    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : script{phaseContext["Script"].to<std::string>()},
          // TODO: try to get the default server URI from DSI
          mongoServerURI{phaseContext["MongoServerURI"].maybe<std::string>().value_or("--nodb")},
          command{phaseContext["Command"].to<std::string>()},
          // This metric tracks the execution time of the script. User can define the MetricsName in
          // the workload yaml file
          operation{phaseContext.operation("DefaultMetricsName", id)},
          // This metric tracks the output of the external script.
          // The external script can write a integer to the stdout and this actor will log it as ms.
          // If nothing is written or the output can't be parsed as integer, this metric will be
          // ignored.
          scriptOperation{phaseContext.namedOperation("ExternalScript", id)} {

            // The script should not have double quotes
            assert(script.find("\"") == std::string::npos);
            if (command == "mongosh") {
                programCommand = "mongosh " + mongoServerURI + " --quiet --eval \"" + script + "\"";
            } else if (command == "sh") {
                programCommand = "sh -c \"" + script + "\"";
            } else {
                throw std::runtime_error("Script type " + command + " is not supported.");
            }
        }
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
        throw std::runtime_error("Execution of command " + std::string(cmd) + " failed!");
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

        BOOST_LOG_TRIVIAL(debug) << "ExternalScriptRunner running " << config->programCommand;

        for (const auto&& _ : config) {

            // Execute the script and read result from stdout
            auto ctx = config->scriptOperation.start();
            const char* programCmdPtr = const_cast<char*>(config->programCommand.c_str());
            std::string result = exec(programCmdPtr);
            ctx.success();

            try {
                // If the result from stdout can be parsed as integer, report the operation metrics
                // Otherwise, ignore the result
                int num = std::stoi(result);
                config->operation.report(metrics::clock::now(), std::chrono::milliseconds{num});
            } catch (std::exception& err) {
                BOOST_LOG_TRIVIAL(debug) << "Command " << config->programCommand
                                         << " wrote non-integer output: " << result;
            }
        }
    }
}

ExternalScriptRunner::ExternalScriptRunner(genny::ActorContext& context)
    // These are the attributes for the actor.
    : Actor{context}, _loop{context, ExternalScriptRunner::id()} {}

namespace {
auto registerExternalScriptRunner = Cast::registerDefault<ExternalScriptRunner>();
}  // namespace
}  // namespace genny::actor
