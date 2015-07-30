#include "parse_util.hpp"

using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::array_context;
using bsoncxx::builder::stream::single_context;

namespace mwg {

void parseMap(document &docbuilder, YAML::Node node) {
  for (auto entry : node) {
      cout << "In parseMap. entry.first: " << entry.first.Scalar()
          << " entry.second " << entry.second.Scalar() << endl;
      // can I just use basic document builder. Open, append, concatenate, etc? 
    if (entry.second.IsMap()) {
      docbuilder << entry.first.Scalar() << open_document;
      parseMap(docbuilder, entry.second);
      docbuilder << close_document;
    } else if (entry.second.IsSequence()) {
        auto arraybuilder  = docbuilder << entry.first.Scalar() << open_array;
        cout << "about to parseSequence" << endl;
        parseSequence(arraybuilder, entry.second);
        cout << "after to parseSequence" << endl;
        arraybuilder << close_array;
        cout << "closed array" << endl;
    } else { // scalar
      docbuilder << entry.first.Scalar() << entry.second.Scalar();
    }
  }
}
    
    void parseSequence(bsoncxx::v0::builder::stream::array_context<bsoncxx::v0::builder::stream::key_context<bsoncxx::v0::builder::stream::closed_context>> &arraybuilder,
                       YAML::Node node) {
        
        for (auto entry : node) {
            if (entry.IsMap()) {
                cout << "Entry isMap" << endl;
                document mydoc{};
                parseMap(mydoc, entry);
                bsoncxx::builder::stream::concatenate doc;
                doc.view = mydoc.view();
                arraybuilder << open_document << doc << close_document;
            } else if (entry.IsSequence()) {
                cerr << "Nest sequences don't work yet" << endl;
                exit(EXIT_FAILURE);
                arraybuilder << open_array;
                parseSequence(arraybuilder, entry);
            } else // scalar
                {
                    cout << "Trying to put entry into array builder " << entry.Scalar() << endl;
                    arraybuilder << entry.Scalar();
                }
        }
    }
    
    
}
