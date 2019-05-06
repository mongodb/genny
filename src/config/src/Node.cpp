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

#include <config/Node.hpp>

#include <boost/core/demangle.hpp>

namespace genny {

std::string InvalidYAMLException::createWhat(const std::string& path,
                                             const YAML::ParserException& yamlException) {
    std::stringstream out;
    out << "Invalid YAML: ";
    out << "'" << yamlException.msg << "' ";
    out << "at (Line:Column)=(" << yamlException.mark.line << ":"
        << yamlException.mark.column << "). ";
    out << "On node with path '" << path << "'.";

    return out.str();
}

std::string InvalidConversionException::createWhat(const Node& node,
                                                   const YAML::BadConversion& yamlException,
                                                   const std::type_info& destType) {
    std::stringstream out;
    out << "Couldn't convert to '" << boost::core::demangle(destType.name()) << "': ";
    out << "'" << yamlException.msg << "' at (Line:Column)=(" << yamlException.mark.line << ":"
        << yamlException.mark.column << "). ";
    out << "On node with path '" << node.path() << "': ";
    out << node;

    return out.str();
}

std::string InvalidKeyException::createWhat(const std::string& msg,
                                            const std::string& key,
                                            const Node* node) {
    std::stringstream out;
    out << "Invalid key '" << key << "': ";
    out << msg << " ";
    out << "On node with path '" << node->path() << "': ";
    out << *node;

    return out.str();
}

void Node::buildPath(std::stringstream &out) const {
    if (_parent) {
        _parent->buildPath(out);
        out << "/";
    }
    out << _key;
}

YAML::Node Node::parse(std::string yaml, std::string path) {
    try {
        return YAML::Load(yaml);
    } catch (const YAML::ParserException& x) {
        BOOST_THROW_EXCEPTION(InvalidYAMLException(path, x));
    }
}

std::string Node::path() const {
    std::stringstream out;
    this->buildPath(out);
    return out.str();
}

Node::iterator Node::begin() const {
    return Node::iterator{_yaml.begin(), this};
}

Node::iterator Node::end() const {
    return Node::iterator{_yaml.end(), this};
}


}  // namespace genny
