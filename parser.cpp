#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array_context.hpp>
#include <bsoncxx/builder/stream/single_context.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include <vector>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "query.hpp"
#include "insert.hpp"
#include "parse_util.hpp"
#include "workload.hpp"

using namespace std;
using namespace mwg;

int main(int argc, char *argv[]) {
  string filename = "sample.yml";
  if (argc > 1)
    filename = argv[1];

  YAML::Node nodes = YAML::LoadFile(filename);

  // cout << nodes << endl;
  // if (nodes["random query"])
  //   cout << "I have random query" << endl;

  // for (auto node : nodes)
  //   cout << node.first << endl;

  // Look for main. And start building from there.
  if (auto main = nodes["main"]) {
    //    cout << "Have main here: " << main << endl;

    mongocxx::instance inst{};
    mongocxx::client conn{};

    // query myquery(workload);
    // myquery.execute(conn);
    workload myworkload(main);
    myworkload.execute(conn);
  }

  else
    cout << "There was no main" << endl;
}
