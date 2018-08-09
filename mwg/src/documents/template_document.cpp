#include "template_document.hpp"

#include <boost/log/trivial.hpp>
#include <stdlib.h>

#include "parse_util.hpp"
#include "value_generators.hpp"

using namespace std;

namespace mwg {

templateDocument::templateDocument(YAML::Node& node) : overrideDocument() {
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "templateDocument constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in templateDocument constructor";
        exit(EXIT_FAILURE);
    }

    auto templates = getGeneratorTypes();
    std::vector<std::tuple<std::string, std::string, YAML::Node>> overrides;

    BOOST_LOG_TRIVIAL(trace) << "In templateDocument constructor";
    doc.setDoc(parseMap(node, templates, "", overrides));
    BOOST_LOG_TRIVIAL(trace)
        << "In templateDocument constructor. Parsed the document. About to deal with overrides";
    for (auto entry : overrides) {
        auto key = std::get<0>(entry);
        auto typeString = std::get<1>(entry);
        YAML::Node yamlOverride = std::get<2>(entry);
        BOOST_LOG_TRIVIAL(trace) << "In templateDocument constructor. Dealing with an override for "
                                 << key;

        auto type = typeString.substr(1, typeString.length());
        BOOST_LOG_TRIVIAL(trace) << "Making value generator for key " << key << " and type "
                                 << type;
        override[key] = makeUniqueValueGenerator(yamlOverride, type);
    }
}
}  // namespace mwg
