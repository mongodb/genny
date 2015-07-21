#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include <vector>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "query.hpp"

using namespace std;
using namespace mwg;

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::finalize;

void parseMap(document &docbuilder, YAML::Node node) {
  for (auto entry : node) {
cout << "In parseMap. entry.first: " << entry.first.Scalar() << " entry.second" << entry.second.Scalar() << endl;
    if (entry.second.IsMap()) {
docbuilder << entry.first.Scalar() << open_document;
      parseMap(docbuilder, entry.second);
docbuilder << close_document;
    } else if (entry.second.IsSequence()) {
    } else { // scalar
docbuilder << entry.first.Scalar() << entry.second.Scalar();
    }
  }
}
void parseSequence(bsoncxx::builder::stream::document &docbuilder,
                   YAML::Node node) {
  for (auto entry : node) {
    if (entry.second.IsMap()) {

    } else if (entry.second.IsSequence()) {
    } else // scalar
    {
    }
  }
}
int main() {
  YAML::Node nodes = YAML::LoadFile("sample.yml");

  // cout << nodes << endl;
  // if (nodes["random query"])
  //   cout << "I have random query" << endl;

  // for (auto node : nodes)
  //   cout << node.first << endl;

  // Look for main. And start building from there.
  if (auto main = nodes["main"]) {
    cout << "Have main here: " << main << endl;
    auto workload = nodes[main.Scalar()];
    if (workload)
      cout << "Have real workload here: " << workload << endl;
    else
      cout << "Error -- workload defined by main doesn't exist" << endl;
    cout << nodes["query"]["query"] << endl;

    document document{};
auto q = nodes["query"]["query"];
cout << "type: " << q.Type() << endl;
if (q.IsMap())
    parseMap(document, nodes["query"]["query"]);
    cout << bsoncxx::to_json(document) << endl;
    mongocxx::instance inst{};
    mongocxx::client conn{};

    query myquery(workload);
    myquery.execute(conn);
  }

  else
    cout << "There was no main" << endl;
}
