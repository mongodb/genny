#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>


using namespace std;

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::array_context;
using bsoncxx::builder::stream::single_context;

namespace mwg {

void parseMap(document &docbuilder, YAML::Node node);

}
