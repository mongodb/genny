#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/client.hpp>

using namespace std;

namespace mwg {
bool isNumber(string);
void parseMap(bsoncxx::builder::stream::document&, YAML::Node);
void parseSequence(bsoncxx::builder::stream::array& arraybuilder, YAML::Node node);
bsoncxx::types::value yamlToValue(YAML::Node);
void parseCreateCollectionOptions(mongocxx::options::create_collection&, YAML::Node);
void parseIndexOptions(mongocxx::options::index&, YAML::Node);
void parseInsertOptions(mongocxx::options::insert&, YAML::Node);
void parseFindOptions(mongocxx::options::find&, YAML::Node);
void parseCountOptions(mongocxx::options::count&, YAML::Node);
void parseInsertOptions(mongocxx::options::insert&, YAML::Node);
void parseAggregateOptions(mongocxx::options::aggregate&, YAML::Node);
void parseBulkWriteOptions(mongocxx::options::bulk_write&, YAML::Node);
void parseDeleteOptions(mongocxx::options::delete_options&, YAML::Node);
void parseDistinctOptions(mongocxx::options::distinct&, YAML::Node);
void parseFindOptions(mongocxx::options::find&, YAML::Node);
void parseFindOneAndDeleteOptions(mongocxx::options::find_one_and_delete&, YAML::Node);
void parseFindOneAndReplaceOptions(mongocxx::options::find_one_and_replace&, YAML::Node);
void parseFindOneAndUpdateOptions(mongocxx::options::find_one_and_update&, YAML::Node);
void parseUpdateOptions(mongocxx::options::update&, YAML::Node);
mongocxx::read_preference parseReadPreference(YAML::Node);
mongocxx::write_concern parseWriteConcern(YAML::Node);
}
