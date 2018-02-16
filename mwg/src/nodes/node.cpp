#include "node.hpp"

#include <boost/log/trivial.hpp>

#include "doAll.hpp"
#include "finish_node.hpp"
#include "forN.hpp"
#include "ifNode.hpp"
#include "join.hpp"
#include "load_file_node.hpp"
#include "opNode.hpp"
#include "random_choice.hpp"
#include "sleep.hpp"
#include "spawn.hpp"
#include "system_node.hpp"
#include "workloadNode.hpp"

namespace mwg {

static int count = 0;

node::node(YAML::Node& ynode) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = find
    stopped = false;
    if (!ynode) {
        BOOST_LOG_TRIVIAL(fatal) << "Find constructor and !ynode";
        exit(EXIT_FAILURE);
    }
    if (!ynode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in find type initializer";
        exit(EXIT_FAILURE);
    }
    if (ynode["name"])
        name = ynode["name"].Scalar();
    else
        name = ynode["type"].Scalar() + std::to_string(count++);  // default name
    if (ynode["next"])
        nextName = ynode["next"].Scalar();
    BOOST_LOG_TRIVIAL(debug) << "In node constructor. Name: " << name << ", nextName: " << nextName;
    if (ynode["print"]) {
        BOOST_LOG_TRIVIAL(debug) << "In node constructor and print";
        text = ynode["print"].Scalar();
    }
}

void node::setNextNode(unordered_map<string, node*>& nodes, vector<shared_ptr<node>>& vectornodes) {
    if (nextName.empty()) {
        // Need default next node.
        // Check if there's a next in the vectornodes. Use that if there is
        // Otherwise use finish node
        BOOST_LOG_TRIVIAL(trace) << "nextName is empty. Using default values";
        // FIXME: The following code is a hack to find the next node in the list
        for (uint i = 0; i < vectornodes.size(); i++)
            if (vectornodes[i].get() == this) {
                BOOST_LOG_TRIVIAL(trace) << "Found node";
                if (i < vectornodes.size() - 1) {
                    BOOST_LOG_TRIVIAL(trace) << "Setting next node to next node in list";
                    auto next = vectornodes[i + 1];
                    nextName = next->name;
                    nextNode = next.get();
                } else {
                    BOOST_LOG_TRIVIAL(trace)
                        << "Node was last in vector. Setting next node to Finish";
                    nextName = "Finish";
                    nextNode = nodes["Finish"];
                }
                return;
            }
        BOOST_LOG_TRIVIAL(fatal) << "In setNextNode and fell through default value";
        exit(0);
    } else {
        nextNode = nodes[nextName];
    }
}

void node::executeNextNode(shared_ptr<threadState> myState) {
    // execute the next node if there is one. Actually only set it up for the runner to call the
    // next node
    // BOOST_LOG_TRIVIAL(debug) << "just executed " << name << ". NextName is " << nextName;
    if (!nextNode) {
        BOOST_LOG_TRIVIAL(fatal) << "nextNode is null for some reason";
        exit(0);
    }
    if (stopped || myState->stopped) {
        myState->currentNode = nullptr;
        BOOST_LOG_TRIVIAL(debug) << "Stopped set";
        return;
    }
    if (name != "Finish" && nextNode) {
        // BOOST_LOG_TRIVIAL(debug) << "About to call nextNode->executeNode";
        // update currentNode in the state.
        myState->currentNode = nextNode;
    } else {
        BOOST_LOG_TRIVIAL(debug) << "Next node is not null, but didn't execute it";
    }
}

void node::executeNode(shared_ptr<threadState> myState) {
    // execute this node
    chrono::high_resolution_clock::time_point start, stop;
    start = chrono::high_resolution_clock::now();
    execute(myState);
    stop = chrono::high_resolution_clock::now();
    myStats.recordMicros(std::chrono::duration_cast<chrono::microseconds>(stop - start));
    if (!text.empty()) {
        BOOST_LOG_TRIVIAL(info) << text;
    }
    // execute the next node if there is one
    executeNextNode(myState);
}

std::pair<std::string, std::string> node::generateDotGraph() {
    return (std::pair<std::string, std::string>{name + " -> " + nextName + ";\n", ""});
}

void node::logStats() {
    if (myStats.getCount() > 0)
        BOOST_LOG_TRIVIAL(info) << "Node: " << name << ", Count=" << myStats.getCount()
                                << ", CountExceptions=" << myStats.getCountExceptions()
                                << ", Avg=" << myStats.getMeanMicros().count()
                                << "us, Min=" << myStats.getMinimumMicros().count()
                                << "us, Max = " << myStats.getMaximumMicros().count()
                                << "us, stddev=" << myStats.getPopStdDev().count();
}

bsoncxx::document::value node::getStats(bool withReset) {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::concatenate;
    bsoncxx::builder::basic::document document;

    // FIXME: This should be cleaner. I think stats is a value and owns it's data, and that could be
    // moved into document
    document.append(concatenate(myStats.getStats(withReset)));
    return make_document(kvp(name, document.view()));
}
void node::stop() {
    stopped = true;
}

node* makeNode(YAML::Node yamlNode) {
    if (!yamlNode.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Node in makeNode is not a yaml map";
        exit(EXIT_FAILURE);
    }
    if (yamlNode["type"].Scalar() == "opNode") {
        return new opNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "random_choice") {
        return new random_choice(yamlNode);
    } else if (yamlNode["type"].Scalar() == "sleep") {
        return new sleepNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "ForN") {
        return new ForN(yamlNode);
    } else if (yamlNode["type"].Scalar() == "finish") {
        return new finishNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "doAll") {
        return new doAll(yamlNode);
    } else if (yamlNode["type"].Scalar() == "join") {
        return new join(yamlNode);
    } else if (yamlNode["type"].Scalar() == "workloadNode") {
        return new workloadNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "ifNode") {
        return new ifNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "spawn") {
        return new Spawn(yamlNode);
    } else if (yamlNode["type"].Scalar() == "system") {
        return new SystemNode(yamlNode);
    } else if (yamlNode["type"].Scalar() == "load_file") {
        return new LoadFileNode(yamlNode);
    } else {
        BOOST_LOG_TRIVIAL(debug) << "In makeNode. Type was " << yamlNode["type"].Scalar()
                                 << ". Defaulting to opNode";
        return new opNode(yamlNode);
    }
}
unique_ptr<node> makeUniqeNode(YAML::Node yamlNode) {
    return unique_ptr<node>(makeNode(yamlNode));
}
shared_ptr<node> makeSharedNode(YAML::Node yamlNode) {
    return shared_ptr<node>(makeNode(yamlNode));
}
}
