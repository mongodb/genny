#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/client.hpp>

using namespace std;

namespace mwg {

void parseMap(bsoncxx::builder::stream::document&, YAML::Node);
void parseSequence(bsoncxx::v0::builder::stream::array& arraybuilder, YAML::Node node);
void parseInsertOptions(mongocxx::options::insert&, YAML::Node);
}
