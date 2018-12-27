#include "generators-private.hh"

#include <stdlib.h>

#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>

#include <gennylib/RNG.hpp>
#include <gennylib/value_generators.hpp>

namespace genny::value_generators {


// parse a YAML Node and make a document of the correct type
std::unique_ptr<DocumentGenerator> makeDoc(const YAML::Node node, genny::DefaultRNG& rng) {
    if (!node) {  // empty document should be BsonDocument
        return std::unique_ptr<DocumentGenerator>{new BsonDocument(node)};
    } else
        return std::unique_ptr<DocumentGenerator>{new TemplateDocument(node, rng)};
};


}  // namespace genny::value_generators
