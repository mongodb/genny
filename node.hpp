#include <iostream>
#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>



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

    string name;
    string nextName;

private:
};
}

