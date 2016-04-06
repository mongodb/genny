#pragma once

#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/client.hpp>
#include <set>

using namespace std;

namespace mwg {
bool isNumber(string);
bool isBool(string);
string quoteIfNeeded(string);
bsoncxx::array::value yamlToValue(YAML::Node);
bsoncxx::document::value parseMap(YAML::Node,
                                  std::set<std::string>,
                                  std::string,
                                  std::vector<std::tuple<std::string, std::string, YAML::Node>>&);
bsoncxx::document::value parseMap(YAML::Node);
bsoncxx::array::value parseSequence(YAML::Node node);
bsoncxx::array::value parseSequence(YAML::Node node,
                                    std::set<std::string>,
                                    std::string,
                                    std::vector<std::tuple<std::string, std::string, YAML::Node>>&);
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
