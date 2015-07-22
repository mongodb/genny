#include <stdio.h>   
#include <stdlib.h>   
#include "sleep.hpp"
#include <random>

namespace mwg {
    sleepNode::sleepNode(YAML::Node &node) {
        // need to set the name
        // these should be made into exceptions
        // should be a map, with type = sleep
        if (!node) {
            cerr << "SleepNode constructor and !node" << endl;
            exit(EXIT_FAILURE);
            }
        if (!node.IsMap()) {
            cerr << "Not map in sleepNode type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        if (node["type"].Scalar() != "sleep") {
                cerr << "SleepNode constructor but yaml entry doesn't have type == sleep" << endl;
                exit(EXIT_FAILURE);
            }
        name = node["name"].Scalar();
        sleeptime = stoi(node["sleep"].Scalar());
        cout << "In sleepNode constructor. Sleep time is " << sleeptime << endl;
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
    }
    
    // Execute the node
    void sleepNode::executeNode(mongocxx::client &conn) {
        cout << "sleepNode.execute. Sleeping for " << sleeptime << " ms" << endl;
        struct timespec t;
        t.tv_sec = (int) (sleeptime/1000);
        t.tv_nsec = ( sleeptime % 1000 ) * 1000 * 1000;
        struct timespec out;
        nanosleep(&t, &out);
        cout << "Slept. Calling next node" << endl;
        if (!nextNode)
            cout << "nextNode is null for some reason" << endl;
        else
            nextNode->executeNode(conn);
    }
}

    




