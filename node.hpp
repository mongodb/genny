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
    virtual void execute(mongocxx::client &) = 0;
    const string getName() {return name;};
    shared_ptr<node> next {nullptr};

    string name;
    string nextName;

private:
};
}

