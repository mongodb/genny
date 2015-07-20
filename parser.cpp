#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include <vector>


#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

using namespace std;

int main() {
  YAML::Node nodes = YAML::LoadFile("sample.yml");

  if (nodes["random query"])
    cout << "I have random query" << endl;

  for (auto node : nodes)
    cout << node.first << endl;

  // Look for main. And start building from there.
  if (auto main = nodes["main"]) {
    cout << "Have main here: " << main << endl;
    auto workload = nodes[main.Scalar()];
    if (workload)
      cout << "Have real workload here: " << workload << endl;
    else
      cout << "Error -- workload defined by main doesn't exist" << endl;
cout << nodes["query"]["query"] << endl;
  }

  else
    cout << "There was no main" << endl;

query myquery{workload};
}
