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
    mongocxx::instance inst{};
    mongocxx::client conn{};
string jsonstring{"{\"x\":1}\n"};
cout << jsonstring.c_str() << endl;


if (bson_utf8_validate(jsonstring.c_str(), jsonstring.length() ,false))
    cout << "Validate utf8" << endl;
 else 
     cout << "invalid utf8" << endl;
bson_error_t bsonerror;
auto mquery = bson_new_from_json((const unsigned char *) jsonstring.c_str(), jsonstring.length(), &bsonerror);
//bson_t* result = bson_new_from_json(
//reinterpret_cast<const uint8_t*>(jsonstring.data()), jsonstring.size(), &bsonerror);
if (mquery)
    cout << "Yeah -- we have some bson" << endl;
 else
     cout << bsonerror.message << endl;

query myquery(workload);
myquery.execute(conn);
  }

  else
    cout << "There was no main" << endl;
}
