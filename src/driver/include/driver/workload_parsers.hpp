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

#ifndef HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED
#define HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED

#include <optional>
#include <boost/filesystem.hpp>
#include <yaml-cpp/yaml.h>

#include <driver/v1/DefaultDriver.hpp>

namespace genny::driver::v1 {

namespace fs = boost::filesystem;

/**
 * Values stored in a Context are tagged with a type to ensure
 * they aren't used incorrectly.
 *
 * If updating, please add to typeNames in workload_parsers.cpp
 * Enums don't have good string-conversion or introspection :(
 */
enum class Type {
    kParameter,
    kActorTemplate,
    kActorInstance,
};

/**
 * Manages scoped context for stored values.
 *
 * Only create using a ContextGuard.
 */
class Context {
public:
    using ContextMap = std::map<std::string, std::pair<YAML::Node, Type>>;
    Context(Context* enclosing) : _enclosing{enclosing} {}

    std::optional<YAML::Node> get(const std::string&, const Type& type);
    void insert(const std::string&, const YAML::Node&, const Type& type);

    // Insert all the values of a node, assuming they are of a type.
    void insert(const YAML::Node&, const Type& type);
    Context* enclosing() {return _enclosing;}

private:
    // A context should be a unique_ptr owned by its recursion level.
    Context* _enclosing;
    ContextMap _values;
};

/**
 * Opens a context, closes it when exiting scope.
 */
class ContextGuard {
public:
    ContextGuard(Context*& contextPtr) : _contextPtr{contextPtr}, 
        _managedContext{std::make_unique<Context>(_contextPtr)} {

        _contextPtr = _managedContext.get();
    }

    ~ContextGuard() {
        _contextPtr = _managedContext->enclosing();
    }

private:
    Context*& _contextPtr;
    std::unique_ptr<Context> _managedContext;
};

/**
 * Parse user-defined workload files into shapes suitable for Genny.
 */
class WorkloadParser {
public:
    // Whether to parse the workload normally or for smoke test.
    enum class Mode { kNormal, kSmokeTest };

    explicit WorkloadParser(fs::path phaseConfigPath)
        : _phaseConfigPath{std::move(phaseConfigPath)} {};

    YAML::Node parse(const std::string& source,
                     DefaultDriver::ProgramOptions::YamlSource =
                         DefaultDriver::ProgramOptions::YamlSource::kFile,
                     Mode mode = Mode::kNormal);

private:
    const fs::path _phaseConfigPath;

    // Contexts are owned by their recursion level.
    // The class-level pointer is just for ease of access.
    Context* _context = nullptr;

    YAML::Node recursiveParse(YAML::Node);

    // The following group of methods handle workload files containing preprocess-able keywords.
    void preprocess(std::string key, YAML::Node value, YAML::Node& out);
    void parseTemplates(YAML::Node);
    YAML::Node parseInstances(YAML::Node);
    YAML::Node parseExternal(YAML::Node);
    YAML::Node replaceParam(YAML::Node);
};

/**
 * Convert a workload YAML into a version for smoke test where every phase
 * of every actor runs with Repeat: 1
 */
class SmokeTestConverter {
public:
    static YAML::Node convert(YAML::Node workloadRoot);
};

}  // namespace genny::driver::v1


#endif  // HEADER_0369D27D_9A68_4981_B344_4BAB3EE09A80_INCLUDED
