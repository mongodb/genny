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

#include <boost/log/trivial.hpp>

#include <driver/workload_parsers.hpp>

#include <gennylib/InvalidConfigurationException.hpp>

namespace genny::driver::v1 {

YAML::Node loadFile(const std::string& source) {
    try {
        return YAML::LoadFile(source);
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Error loading yaml from " << source << ": " << ex.what();
        throw;
    }
}

std::map<Type, std::string> typeNames = {
    {Type::kParameter, "Parameter"}, 
    {Type::kActorTemplate, "ActorTemplate"}, 
    {Type::kActorInstance, "ActorInstance"}
};


std::optional<YAML::Node> Context::get(const std::string& name, const Type& type) {
    if (auto val = _values.find(name); val != _values.end()) {
        Type expected = val->second.second;
        if (expected != type) {
            auto os = std::ostringstream();
            os << "Type mismatch for node named " << name 
               << ". Expected " << typeNames[expected] << " but received " 
               << typeNames[type] << ".";
            throw InvalidConfigurationException(os.str());
        }
        return val->second.first;
    } else if (_enclosing) {
        return _enclosing->get(name, type);
    } else {
        return std::nullopt;
    }
}

void Context::insert(const std::string& name, const YAML::Node& val, const Type& type) {
    _values.insert_or_assign(name, std::make_pair(val, type));
}
void Context::insert(const YAML::Node& node, const Type& type) {
    if (node.Type() != YAML::NodeType::Map) {
        auto os = std::ostringstream();
        os << "Invalid context storage of node: " << node
           << ". Please ensure this node is a map rather than a sequence.";
        throw InvalidConfigurationException(os.str());
    }

    for (auto kvp : node) {
        insert(kvp.first.as<std::string>(), kvp.second, type);
    }
}

YAML::Node WorkloadParser::parse(const std::string& source,
                                 const DefaultDriver::ProgramOptions::YamlSource sourceType,
                                 const Mode mode) {
    ContextGuard guard(_context);
    YAML::Node workload;
    if (sourceType == DefaultDriver::ProgramOptions::YamlSource::kString) {
        workload = YAML::Load(source);
    } else {
        workload = loadFile(source);
    }

    auto parsed = recursiveParse(workload);

    switch (mode) {
        case Mode::kSmokeTest:
            return SmokeTestConverter::convert(parsed);
        case Mode::kNormal:
            return parsed;
        default:
            // Getting here should be a compile error.
            throw InvalidConfigurationException("Unknown workload parse mode");
    }
}

YAML::Node WorkloadParser::recursiveParse(YAML::Node node) {
    YAML::Node out;
    switch (node.Type()) {
        case YAML::NodeType::Map: {
            for (auto kvp : node) {
                preprocess(kvp.first.as<std::string>(), kvp.second, out);
            }
            break;
        }
        case YAML::NodeType::Sequence: {
            for (auto val : node) {
                out.push_back(recursiveParse(val));
            }
            break;
        }
        default:
            return node;
    }
    return out;
}

YAML::Node WorkloadParser::replaceParam(YAML::Node input) {
    if (!input["Name"] || !input["Default"]) {
        auto os = std::ostringstream();
        os << "Invalid keys for '^Parameter', please set 'Name' and 'Default' in following node"
           << YAML::Dump(input);
        throw InvalidConfigurationException(os.str());
    }

    auto name = input["Name"].as<std::string>();
    // The default value is mandatory.
    auto defaultVal = input["Default"];

    // Nested params are ignored for simplicity.
    if (auto paramVal = _context->get(name, Type::kParameter)) {
        return *paramVal;
    } else {
        return input["Default"];
    }
}

void WorkloadParser::preprocess(std::string key, YAML::Node value, YAML::Node& out) {
    if (key == "^Parameter") {
        out = replaceParam(value);
    } else if (key == "ActorTemplates") {
        parseTemplates(value);
    } else if (key == "ActorInstance") {
        out = parseInstance(value);   
    } else if (key == "OnlyIn") {
        out = parseOnlyIn(value);
    } else if (key == "ExternalPhaseConfig") {
        auto external = parseExternal(value);
        // Merge the external node with the any other parameters specified
        // for this node like "Repeat" or "Duration".
        for (auto externalKvp : external) {
            if (!out[externalKvp.first])
                out[externalKvp.first] = externalKvp.second;
        }
    } else {
        out[key] = recursiveParse(value);
    }
}

void WorkloadParser::parseTemplates(YAML::Node templates) {
    for (auto temp : templates) {
        _context->insert(temp["TemplateName"].as<std::string>(), temp["Config"], Type::kActorTemplate);
    }
}

YAML::Node WorkloadParser::parseOnlyIn(YAML::Node onlyIn) {
    YAML::Node out;
    YAML::Node nop;
    nop["Nop"] = true;
    int max = recursiveParse(onlyIn["Max"]).as<int>();
    for (int i = 0; i < max; i++) {
        bool isActivePhase = false;
        for (auto activePhase : recursiveParse(onlyIn["Active"])) {
            if (activePhase.as<int>() == i) {
                out.push_back(recursiveParse(onlyIn["Config"]));
                isActivePhase = true;
                break;
            }
        }
        if (!isActivePhase) {
           out.push_back(nop);
        }
    }
    return out;
}

YAML::Node WorkloadParser::parseInstance(YAML::Node instance) {
    YAML::Node actor;

    {
        ContextGuard guard(_context);
        auto temp = _context->get(instance["Template"].as<std::string>(), Type::kActorTemplate);
        if (!temp) {
            auto os = std::ostringstream();
            os << "Expected template named " << instance["Template"].as<std::string>()
               << " but could not be found.";
            throw InvalidConfigurationException(os.str());
        }
        _context->insert(instance["Parameters"], Type::kParameter);
        actor = recursiveParse(*temp);
    }
    
    return actor;
}

YAML::Node WorkloadParser::parseExternal(YAML::Node external) {
    ContextGuard guard(_context);
    int keysSeen = 0;

    if (!external["Path"]) {
        std::ostringstream stm;
        stm << "Missing the `Path` top-level key in your external phase configuration: ";
        stm << YAML::Dump(external);
        throw InvalidConfigurationException(stm.str());
    }
    fs::path path(external["Path"].as<std::string>());
    keysSeen++;

    path = fs::absolute(_phaseConfigPath / path);

    if (!fs::is_regular_file(path)) {
        auto os = std::ostringstream();
        os << "Invalid path to external PhaseConfig: " << path
           << ". Please ensure your workload file is placed in 'workloads/[subdirectory]/' and the "
              "'Path' parameter is relative to the 'phases/' directory";
        throw InvalidConfigurationException(os.str());
    }

    auto replacement = loadFile(path.string());

    // Block of code for parsing the schema version.
    {
        if (!replacement["PhaseSchemaVersion"]) {
            throw InvalidConfigurationException(
                "Missing the `PhaseSchemaVersion` top-level key in your external phase "
                "configuration");
        }
        auto phaseSchemaVersion = replacement["PhaseSchemaVersion"].as<std::string>();
        if (phaseSchemaVersion != "2018-07-01") {
            auto os = std::ostringstream();
            os << "Invalid phase schema version: " << phaseSchemaVersion
               << ". Please ensure the schema for your external phase config is valid and the "
                  "`PhaseSchemaVersion` top-level key is set correctly";
            throw InvalidConfigurationException(os.str());
        }

        // Delete the schema version instead of adding it to `keysSeen`.
        replacement.remove("PhaseSchemaVersion");
    }

    if (external["Parameters"]) {
        keysSeen++;
        _context->insert(external["Parameters"], Type::kParameter);
    }

    if (external["Key"]) {
        keysSeen++;
        const auto key = external["Key"].as<std::string>();
        if (!replacement[key]) {
            auto os = std::ostringstream();
            os << "Could not find top-level key: " << key << " in phase config YAML file: " << path;
            throw InvalidConfigurationException(os.str());
        }
        replacement = replacement[key];
    }

    if (external.size() != keysSeen) {
        auto os = std::ostringstream();
        os << "Invalid keys for 'External'. Please set 'Path' and if any, 'Parameters' in the YAML "
              "file: "
           << path << " with the following content: " << YAML::Dump(external);
        throw InvalidConfigurationException(os.str());
    }

    return recursiveParse(replacement);
}

YAML::Node convertObjForSmokeTest(YAML::Node inNode) {
    YAML::Node out;
    for (auto kvp : inNode) {
        auto&& key = kvp.first.as<std::string>();
        auto&& value = kvp.second;
        if (key == "Duration" || key == "Repeat") {
            out["Repeat"] = 1;
        } else if (key == "GlobalRate" || key == "SleepBefore" || key == "SleepAfter") {
            // Ignore those keys in smoke tests.
        } else {
            out[key] = value;
        }
    }
    return out;
}


YAML::Node SmokeTestConverter::convert(YAML::Node workloadRoot) {
    YAML::Node actorsOut;

    // Convert keywords in the "Actors" block.
    for (auto actor : workloadRoot["Actors"]) {
        YAML::Node actorOut = convertObjForSmokeTest(actor);
        YAML::Node phasesOut;

        // Convert keywords in the "Phases" block.
        for (auto phase : actorOut["Phases"]) {
            phasesOut.push_back(convertObjForSmokeTest(phase));
        }

        actorOut["Phases"] = phasesOut;
        actorsOut.push_back(actorOut);
    }

    workloadRoot["Actors"] = actorsOut;

    return workloadRoot;
}
}  // namespace genny::driver::v1
