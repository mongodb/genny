#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>
#include <random>
#include <unordered_map>


namespace genny::generators {

class DocumentGenerator {
public:
    virtual ~DocumentGenerator(){};
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc) {
        return doc.view();
    };
};


// parse a YAML Node and make a document of the correct type
std::unique_ptr<DocumentGenerator> makeDoc(const YAML::Node, std::mt19937_64&);

}  // namespace genny::generators

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
