#include <iostream>
#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <unordered_map>


#pragma once
using namespace std;

namespace mwg {

class node {

public: 

    node() {};
    virtual ~node() = default;
    node(const node&) = default;
    node(node&&) = default;
    // Execute the node
    virtual void executeNode(mongocxx::client &);
    virtual void execute(mongocxx::client &) {};
    const string getName() {return name;};
    shared_ptr<node> nextNode {nullptr};

    // Set the next node pointer
    virtual void setNextNode(unordered_map<string,shared_ptr<node>> &);

    string name;
    string nextName;

private:
};
}

