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

namespace genny {

void Node::appendKey(std::ostringstream& out) const {
    if (_parent) {
        _parent->appendKey(out);
        out << "/";
    }
    out << _key;
}

YAML::Node Node::parse(std::string yaml) {
    // TODO: better error message if this fails
    return YAML::Load(yaml);
}

std::string Node::path() const {
    std::ostringstream out;
    this->appendKey(out);
    return out.str();
}

Node::iterator Node::begin() const {
    return Node::iterator{_yaml.begin(), this};
}

Node::iterator Node::end() const {
    return Node::iterator{_yaml.end(), this};
}


}
