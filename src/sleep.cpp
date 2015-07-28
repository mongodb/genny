#include <stdio.h>   
#include <stdlib.h>   
#include "sleep.hpp"
#include <random>
#include "parse_util.hpp"
#include <time.h>

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
        sleeptime = node["sleep"].as<uint64_t>();
        cout << "In sleepNode constructor. Sleep time is " << sleeptime << endl;
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
    }
    
    // Execute the node
    void sleepNode::executeNode(mongocxx::client &conn, mt19937_64 &rng) {
        cout << "sleepNode.execute. Sleeping for " << sleeptime << " ms" << endl;
        struct timespec t;
        t.tv_sec = (int) (sleeptime/1000);
        t.tv_nsec = ( sleeptime % 1000 ) * 1000 * 1000;
        struct timespec out;
        nanosleep(&t, &out);
        cout << "Slept. Calling next node" << endl;
        auto next = nextNode.lock();
        if (!next)
            cout << "nextNode is null for some reason" << endl;
        else if (!stopped)
            next->executeNode(conn,rng);
    }
}

    




