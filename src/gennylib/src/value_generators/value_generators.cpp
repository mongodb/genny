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

#include "generators-private.hh"

#include <stdlib.h>

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>

#include <gennylib/DefaultRandom.hpp>
#include <gennylib/value_generators.hpp>

namespace genny::value_generators {


// parse a YAML Node and make a document of the correct type
std::unique_ptr<DocumentGenerator> makeDoc(const YAML::Node node, genny::DefaultRandom& rng) {
    if (!node) {  // empty document should be BsonDocument
        return std::unique_ptr<DocumentGenerator>{new BsonDocument(node)};
    } else
        return std::unique_ptr<DocumentGenerator>{new TemplateDocument(node, rng)};
};


}  // namespace genny::value_generators
