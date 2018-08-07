#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>

namespace genny {

class document {
public:
    virtual ~document() = 0;
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc, threadState&) {
        return doc.view();
    };
};

class bsonDocument : public document {
public:
    bsonDocument();
    bsonDocument(YAML::Node&);

    void setDoc(bsoncxx::document::value value) {
        doc = value;
    }
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         threadState&) override;

private:
    std::optional<bsoncxx::document::value> doc;
};

class templateDocument : public document {
public:
    templateDocument();
    templateDocument(YAML::Node&);
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&) override;

protected:
    // The document to override
    bsonDocument doc;
    unordered_map<string, unique_ptr<ValueGenerator>> override;

private:
    // apply the overides, one level at a time
    void applyOverrideLevel(bsoncxx::builder::stream::document&,
                            bsoncxx::document::view,
                            string,
                            threadState&);
};
}  // namespace genny


#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
