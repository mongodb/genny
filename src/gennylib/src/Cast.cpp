// Copyright 2018 MongoDB Inc.
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

#include <gennylib/Cast.hpp>

#include <sstream>

namespace genny {
void Cast::add(const std::string_view& castName, std::shared_ptr<ActorProducer> entry) {
    const auto& [it, success] = _producers.emplace(castName, entry);

    if (!success) {
        std::ostringstream stream;
        auto previousActorProducer = it->second;
        stream << "Failed to add '" << entry->name() << "' as '" << castName
               << "', a producer named '" << previousActorProducer->name()
               << "' was already added instead.";
        throw std::domain_error(stream.str());
    }
}

std::ostream& Cast::streamProducersTo(std::ostream& out) const {
    for (const auto& [castName, producer] : globalCast().getProducers()) {
        out << castName << " is " << producer->name() << std::endl;
    }
    return out;
}
}  // namespace genny
