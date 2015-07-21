#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bson.h>
#include "parse_util.hpp"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "node.hpp"

using namespace std;

namespace mwg {

    class insert : public node {

public: 

    insert(YAML::Node &);
    insert() = delete;
    virtual ~insert() = default;
    insert(const insert&) = default;
    insert(insert&&) = default;
    // Execute the node
    void execute(mongocxx::client &);
    const string getName() {return name;};

private:
    bsoncxx::builder::stream::document insertdoc{};
    string name;
};
}
