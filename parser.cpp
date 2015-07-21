#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include <vector>


#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "query.hpp"

using namespace std;
using namespace mwg;

int main() {
  YAML::Node nodes = YAML::LoadFile("sample.yml");

//cout << nodes << endl;
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

    bsoncxx::builder::stream::document document{};
    for (auto node : nodes["query"]["query"]) {
      cout << "builder " << node.first << " " << node.second << endl;
document << node.first.Scalar() <<  node.second.Scalar();
    }
cout << bsoncxx::to_json(document) << endl;
    mongocxx::instance inst{};
    mongocxx::client conn{};

query myquery(workload);
myquery.execute(conn);
  }

  else
    cout << "There was no main" << endl;
}
