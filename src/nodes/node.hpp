#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <unordered_map>
#include <random>
#include "threadState.hpp"
#include "yaml-cpp/yaml.h"

#pragma once
using namespace std;

namespace mwg {

class node {
public:
    node() : stopped(false){};
    node(YAML::Node&);
    virtual ~node() = default;
    node(const node&) = default;
    node(node&&) = default;
    // Execute the node
    virtual void executeNode(shared_ptr<threadState>);
    virtual void executeNextNode(shared_ptr<threadState>);
    virtual void execute(shared_ptr<threadState>){};
    const string getName() {
        return name;
    };

    void stop() {
        stopped = true;
    };
    // Set the next node pointer
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&);
    string name;
    string nextName;

protected:
    weak_ptr<node> nextNode;
    bool stopped;
    string text;
};

void runThread(shared_ptr<node> Node, shared_ptr<threadState> myState);
}
