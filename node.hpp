#include <iostream>
#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <unordered_map>
#include <random>


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
    virtual void executeNode(mongocxx::client &, mt19937_64 &);
    virtual void execute(mongocxx::client &, mt19937_64 &) {};
    const string getName() {return name;};
    shared_ptr<node> nextNode {nullptr};

    // Set the next node pointer
    virtual void setNextNode(unordered_map<string,shared_ptr<node>> &);

    string name;
    string nextName;

private:
};
}

