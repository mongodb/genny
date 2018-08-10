#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>
#include <random>
#include <unordered_map>

namespace genny::value_generators {

/*
 * This is the base class for all document generrators. A document generator generates a possibly
 * random bson view that can be used in generating interesting mongodb requests.
 *
 */
class DocumentGenerator {
public:
    virtual ~DocumentGenerator(){};

    /*
     * @param doc
     *  The bson stream builder used to hold state for the view. The view lifetime is tied to that
     * doc.
     *
     * @return
     *  Returns a bson view of the generated document.
     */
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc) {
        return doc.view();
    };
};


/*
 * Factory function to parse a YAML Node and make a document generator of the correct type.
 *
 * @param Node
 *  The YAML node with the configuration for this document generator.
 * @param RNG
 *  A reference to the random number generator for the owning thread. Internal object may save a
 * reference to this random number generator.
 */
std::unique_ptr<DocumentGenerator> makeDoc(const YAML::Node, std::mt19937_64&);

}  // namespace genny::value_generators

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
