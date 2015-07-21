#include <iostream>
#include <string>


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
    void execute(mongocxx::client &);
    const string getName() {return name;};

private:
    shared_ptr<node> next {nullptr};
    string name;
};
}

