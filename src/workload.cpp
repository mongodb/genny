#include "workload.hpp"
#include <stdlib.h>
#include <iostream>

#include <mongocxx/instance.hpp>

#include "node.hpp"
#include "insert.hpp"
#include "query.hpp"
#include "random_choice.hpp"
#include "sleep.hpp"
#include "finish_node.hpp"
#include "forN.hpp"

namespace mwg {
    workload::workload(YAML::Node &inputNodes)  : stopped (false) {
        if (!inputNodes) {
            cerr << "Workload constructor and !nodes" << endl;
            exit(EXIT_FAILURE);
            }
        unordered_map<string,shared_ptr<node>> nodes;
        YAML::Node yamlNodes;
        if (inputNodes.IsMap()) {
            // read out things like the seed
            yamlNodes = inputNodes["nodes"];
            if (!yamlNodes.IsSequence()) {
                cerr << "Workload is a map, but nodes is not sequnce in workload type initializer " << endl;
                exit(EXIT_FAILURE);
            } 
            if (inputNodes["seed"])
                rng.seed(strtod(inputNodes["seed"].Scalar().c_str(), NULL));
            name = inputNodes["name"].Scalar();
            cout << "In workload constructor, and was passed in a map. Name: " << name << " and seed: " << strtod(inputNodes["seed"].Scalar().c_str(), NULL) << endl;
        }
        else if (inputNodes.IsSequence()) {
            yamlNodes = inputNodes;
        } 
        else {
            cerr << "Not sequnce in workload type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        if (!yamlNodes.IsSequence()) {
            cerr << "Not sequnce in workload type initializer after passing through map or sequence processing. Type is " << yamlNodes.Type() << endl;
            exit(EXIT_FAILURE);
        }
            
        for (auto yamlNode : yamlNodes) {
            if (!yamlNode.IsMap()) {
                cerr << "Node in workload is not a yaml map" << endl;
                exit(EXIT_FAILURE);
            }
            if (yamlNode["type"].Scalar() == "query" ) {
                auto mynode = make_shared<query> (yamlNode);
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added query node" << endl;
            }
            else if (yamlNode["type"].Scalar() == "insert") {
                auto mynode = make_shared<insert> (yamlNode);
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added insert node" << endl;
            }
            else if (yamlNode["type"].Scalar() == "random_choice") {
                auto mynode = make_shared<random_choice> (yamlNode);
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added random_choice node" << endl;
            }
            else if (yamlNode["type"].Scalar() == "sleep") {
                auto mynode = make_shared<sleepNode> (yamlNode);
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added sleep node" << endl;
            }
            else if (yamlNode["type"].Scalar() == "forN") {
                auto mynode = make_shared<forN> (yamlNode);
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added sleep node" << endl;
            }
            else if (yamlNode["type"].Scalar() == "finish") {
                auto mynode = make_shared<finishNode> (yamlNode);
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added finish node" << endl;
            }
            else
                cerr << "Don't know how to handle workload node with type " << yamlNode["type"] << endl;
        }

        cout << "Added all the nodes in yamlNode" << endl;
        // Add an implicit finish_node if it doesn't exist
        if (!nodes["Finish"])
            {
                auto mynode = make_shared<finishNode> ();
                nodes[mynode->getName()] = mynode;
                // this is an ugly hack for now
                vectornodes.push_back(mynode);
                cout << "In workload constructor and added implicit finish node" << endl;
                
            }

        // link the things together
        for (auto mnode : vectornodes) {
            cout << "Setting next node for " << mnode->getName() << ". Next node name is " << mnode->nextName << endl;
            mnode->setNextNode(nodes);
        }
    }
    void workload::execute(mongocxx::client &conn) {
        //        for (auto mnode : vectornodes) {
        //    mnode->execute(conn);}
        vectornodes[0]->executeNode(conn, rng);
    }

    void workload::stop () {
        stopped = true;
        for (auto mnode : vectornodes)
            mnode->stop();
    };


}
