#include "generators-private.hh"
#include <boost/log/trivial.hpp>
#include <bsoncxx/json.hpp>
#include <gennylib/generators.hpp>
#include <random>
#include <stdlib.h>

namespace genny::generators {


// parse a YAML Node and make a document of the correct type
std::unique_ptr<DocumentGenerator> makeDoc(const YAML::Node node, std::mt19937_64& rng) {
    if (!node) {  // empty document should be BsonDocument
        return std::unique_ptr<DocumentGenerator>{new BsonDocument(node)};
    } else
        return std::unique_ptr<DocumentGenerator>{new TemplateDocument(node, rng)};
};

// This returns a set of the value generator types with $ prefixes
const std::set<std::string> getGeneratorTypes() {
    return (std::set<std::string>{"$randomint", "$fastrandomstring", "$randomstring", "$useval"});
}


}  // namespace genny::generators
