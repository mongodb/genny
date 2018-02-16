#pragma once

#include <memory>
#include <vector>

#include "bsonDocument.hpp"
#include "document.hpp"
#include "value_generator.hpp"

using namespace std;

namespace mwg {
class AppendDocument : public document {
public:
    AppendDocument() : document(){};
    AppendDocument(YAML::Node&);

    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         threadState&) override;

protected:
    // The document to override
    bsonDocument doc;
    vector<std::pair<string, unique_ptr<ValueGenerator>>> appends;

private:
    // apply the overides, one level at a time
    void applyOverrideLevel(bsoncxx::builder::stream::document&,
                            bsoncxx::document::view,
                            string,
                            threadState&);
};
}
