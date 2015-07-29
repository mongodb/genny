#include "parse_util.hpp"

namespace mwg {

void parseMap(document &docbuilder, YAML::Node node) {
  for (auto entry : node) {
    // cout << "In parseMap. entry.first: " << entry.first.Scalar()
    //      << " entry.second" << entry.second.Scalar() << endl;
      // can I just use basic document builder. Open, append, concatenate, etc? 
    if (entry.second.IsMap()) {
      docbuilder << entry.first.Scalar() << open_document;
      parseMap(docbuilder, entry.second);
      docbuilder << close_document;
    } else if (entry.second.IsSequence()) {
        
        parseSequence(docbuilder, entry.first.Scalar(), entry.second);
    } else { // scalar
      docbuilder << entry.first.Scalar() << entry.second.Scalar();
    }
  }
}
    
void parseSequence(document &docbuilder, string Name,
                       YAML::Node node) {
        
        auto arraybuilder = docbuilder << Name << open_array;
        for (auto entry : node) {
            if (entry.second.IsMap()) {
                document mydoc{};
                parseMap(mydoc, entry.second);
                bsoncxx::builder::stream::concatenate doc;
                doc.view = mydoc.view();
                arraybuilder << doc;
            } else if (entry.second.IsSequence()) {
            } else // scalar
                {
                    arraybuilder << entry.first.Scalar();
                }
            arraybuilder << close_array;
        }
    }
    
    
}
