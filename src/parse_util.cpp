#include "parse_util.hpp"

namespace mwg {

void parseMap(document &docbuilder, YAML::Node node) {
  for (auto entry : node) {
    // cout << "In parseMap. entry.first: " << entry.first.Scalar()
    //      << " entry.second" << entry.second.Scalar() << endl;
    if (entry.second.IsMap()) {
      docbuilder << entry.first.Scalar() << open_document;
      parseMap(docbuilder, entry.second);
      docbuilder << close_document;
    } else if (entry.second.IsSequence()) {
// sequences tbd
    } else { // scalar
      docbuilder << entry.first.Scalar() << entry.second.Scalar();
    }
  }
}
// sequences tbd
// void parseSequence(array_context<single_context>  &docbuilder,
//                    YAML::Node node) {
//   for (auto entry : node) {
//     if (entry.second.IsMap()) {
// // docbuilder << open_document;
// // parseMap(docbuilder, entry.second);
// // docbuilder << close_document;
//     } else if (entry.second.IsSequence()) {
//     } else // scalar
//     {
//       array << entry.first.Scalar();
//     }
// docbuilder << close_array;
//   }
// }


}
