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

#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include <cast_core/actors/ExternalScriptRunner.hpp>

namespace genny::actor {

class TempScriptFile {
public:
    TempScriptFile(const std::string& script) {
        boost::filesystem::path temp =
            boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
        _path = temp.native();
        std::ofstream file{_path};
        file << script;
    }

    TempScriptFile(const std::string& script, const std::string& path)
        : _path{path} {
        std::ofstream file{_path};
        file << script;
    }

    TempScriptFile(const TempScriptFile&) = delete;

    ~TempScriptFile() {
        std::remove(_path.c_str());
    }

    const std::string& path() {
        return _path;
    }

private:
    std::string _path;
};

class ScriptRunner {
public:
    ScriptRunner(PhaseContext& phaseContext, ActorId id, const std::string& workloadPath)
        : _workloadPath{workloadPath},
          _scriptOperation{phaseContext.namedOperation("ExternalScript", id)}{}
    virtual ~ScriptRunner() = default;
    virtual std::string runScript() = 0;

    const std::string& workloadPath() const {
        return _workloadPath;
    }

protected:
    std::string invoke(const std::string& invocation) {
        // Execute the script and read result from stdout
        auto ctx = _scriptOperation.start();
        const char* programCmdPtr = const_cast<char*>(invocation.c_str());
        std::string result = exec(programCmdPtr);
        ctx.success();
        return result;
    }

private:
    /**
     * @brief Execute external script
     *
     * @param cmd
     * @return std::string
     */
    std::string exec(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        BOOST_LOG_TRIVIAL(info) << "Running command: " << cmd;

        FILE* pipe = popen(cmd, "r");
        if (!pipe) {
            throw std::runtime_error("Execution of command " + std::string(cmd) + " failed!");
        }
        try {
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
                BOOST_LOG_TRIVIAL(info) << "Script output: " << buffer.data();
            }
        } catch (...) {
            // We want to ensure the pipe is closed if any exception occurs,
            // but don't want to handle the exception here. As such, we re-throw
            pclose(pipe);
            throw;
        }

        int pcloseResult = pclose(pipe);
        int exitStatus = WEXITSTATUS(pcloseResult);
        if (exitStatus != 0) {
            throw std::runtime_error("Script exited with non-zero exit code " +
                                     std::to_string(exitStatus));
        }

        return result;
    }

    // Record data of the script operation itself
    metrics::Operation _scriptOperation;
    const std::string& _workloadPath;
};

class GeneralRunner: public ScriptRunner {
public:
    GeneralRunner(PhaseContext& phaseContext, ActorId id, const std::string& workloadPath)
        : ScriptRunner(phaseContext, id, workloadPath),
          _script{phaseContext["Script"].to<std::string>()} {
        std::string command{phaseContext["Command"].to<std::string>()};
        if (command == "mongosh") {
            _invocation = "mongosh " + phaseContext["MongoServerURI"].maybe<std::string>().value_or("--nodb") + " --quiet --file";
        } else if (command == "sh") {
            // No --file argument is required here, the script is run like
            // sh /path/to/file
            _invocation = "sh";
        } else {
            throw std::runtime_error("Script type " + command + " is not supported.");
        }
    }

    virtual std::string runScript() override {
        TempScriptFile file{_script};
        std::stringstream invocation;
        // Note we append 2>&1 to the script so that stderr shows up in the output
        // as the way we're invoking the script results in us only reading stdout
        invocation << _invocation << " " << file.path() << " 2>&1";
        return invoke(invocation.str());
    }

private:
    std::string _invocation;
    std::string _script;
};

/*
Note we've specifically split out python support as it has some extra built-in
features in genny to make python actors have first-class support (beyond smaller
shell and js scripts, which the general external script runner covers).
*/
class PythonRunner: public ScriptRunner {
public:
    PythonRunner(PhaseContext& phaseContext, ActorId id, const std::string& workloadPath)
        : ScriptRunner(phaseContext, id, workloadPath),
          _module{phaseContext["Module"].to<std::string>()},
          _endpoint{phaseContext["Endpoint"].to<std::string>()} {}

    virtual std::string runScript() override {
        std::stringstream invocation;
        invocation << "python -m " << _module
            << " " << _endpoint << " " << workloadPath() << " 2>&1";
        return invoke(invocation.str());
    }

private:
    std::string _module;
    std::string _endpoint;
};


struct ExternalScriptRunner::PhaseConfig {

    // This metric tracks the output of the external script.
    // The external script can write a integer to the stdout and this actor will log it as ms.
    // If nothing is written or the output can't be parsed as integer, this metric will be
    // ignored.
    metrics::Operation operation;

    PhaseConfig(PhaseContext& phaseContext, ActorId id, const std::string& workloadPath, const std::string& type)
        : operation{phaseContext.operation("DefaultMetricsName", id)} {
            if(type == "Python") {
                _scriptRunner = std::make_unique<PythonRunner>(phaseContext, id, workloadPath);
            } else {
                _scriptRunner = std::make_unique<GeneralRunner>(phaseContext, id, workloadPath);
            }
        }
    std::string runScript() {
        return _scriptRunner->runScript();
    }
private:
    std::unique_ptr<ScriptRunner> _scriptRunner;
};


void ExternalScriptRunner::run() {
    // This section gets executed before the phases. Setup is executed here, so maybe
    // we could add some sanity checks before actually running the shell.

    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            std::string result = config->runScript();
            try {
                // If the result from stdout can be parsed as integer, report the operation metrics
                // Otherwise, ignore the result
                int num = std::stoi(result);
                config->operation.report(metrics::clock::now(), std::chrono::milliseconds{num});
            } catch (std::exception& err) {
                BOOST_LOG_TRIVIAL(debug)
                    << "Command wrote non-integer output: " << result << " " << err.what();
            }
        }
    }
}

ExternalScriptRunner::ExternalScriptRunner(genny::ActorContext& context)
    // These are the attributes for the actor.
    : Actor{context},
      _loop{context, ExternalScriptRunner::id(), context.workload().workloadPath(), context["Type"].to<std::string>()}{}

namespace {
auto registerExternalScriptRunner = Cast::registerDefault<ExternalScriptRunner>();

auto pythonCommandProducer =
    std::make_shared<DefaultActorProducer<ExternalScriptRunner>>("Python");
auto registerPythonActor = Cast::registerCustom(pythonCommandProducer);
}  // namespace
}  // namespace genny::actor
