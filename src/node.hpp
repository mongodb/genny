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

    node() : stopped(false) {};
    virtual ~node() = default;
    node(const node&) = default;
    node(node&&) = default;
    // Execute the node
    virtual void executeNode(mongocxx::client &, mt19937_64 &);
    virtual void execute(mongocxx::client &, mt19937_64 &) {};
    const string getName() {return name;};

    void stop () {stopped = true;};
    // Set the next node pointer
    virtual void setNextNode(unordered_map<string,shared_ptr<node>> &);
    string name;
    string nextName;

protected:
    weak_ptr<node> nextNode;
    bool stopped;
};

}

