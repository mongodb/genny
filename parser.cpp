#include <iostream>
#include <string>
#include "yaml-cpp/yaml.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

using namespace std;

int main()
{
    YAML::Node nodes = YAML::LoadFile("sample.yml");

    if (nodes["random query"] ) 
        cout << "I have random query" << endl;

    for (auto node : nodes)
        cout << node.first << endl;

    //    cout << nodes << endl;

}
