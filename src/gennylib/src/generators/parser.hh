
#ifndef HEADER_555BBB4B_4C7D_434E_8CB9_67990FAF0947
#define HEADER_555BBB4B_4C7D_434E_8CB9_67990FAF0947

#include <set>

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <yaml-cpp/yaml.h>

namespace genny::generators::parser {
bool isNumber(std::string);
bool isBool(std::string);
std::string quoteIfNeeded(std::string);
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
}  // namespace genny::generators::parser

#endif  // HEADER_555BBB4B_4C7D_434E_8CB9_67990FAF0947
