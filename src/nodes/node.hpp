#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <unordered_map>
#include <random>
#include "threadState.hpp"
#include "yaml-cpp/yaml.h"
#include <memory>
#include "stats.hpp"
#include <atomic>

#pragma once
using namespace std;

namespace mwg {

class node {
public:
    node() : stopped(false){};
    node(YAML::Node&);
    virtual ~node() = default;
    // do we use the copy constructor at all?
    node(const node& other) : nextNode(other.nextNode), stopped(false), text(other.text) {
        myStats.reset();
    };
    node(node&&) = default;
    // Execute the node
    virtual void executeNode(shared_ptr<threadState>);
    virtual void executeNextNode(shared_ptr<threadState>);
    virtual void execute(shared_ptr<threadState>){};
    const string getName() {
        return name;
    };

    virtual void stop();
    // Set the next node pointer
    virtual void setNextNode(unordered_map<string, shared_ptr<node>>&, vector<shared_ptr<node>>&);
    string name;
    string nextName;
    // Generate a dot file for generating a graph.
    virtual std::pair<std::string, std::string> generateDotGraph();
    virtual void logStats();                                    // print out the stats
    virtual bsoncxx::document::value getStats(bool withReset);  // bson object with stats


protected:
    weak_ptr<node> nextNode;
    std::atomic<bool> stopped;
    string text;
    stats myStats;
};

void runThread(shared_ptr<node> Node, shared_ptr<threadState> myState);
unique_ptr<node> makeUniqeNode(YAML::Node);
shared_ptr<node> makeSharedNode(YAML::Node);
}
