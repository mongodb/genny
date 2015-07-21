#include "workload.hpp"
#include<stdlib.h>

namespace mwg {
    workload::workload(YAML::Node &yamlNodes) {
        if (!yamlNodes) {
            cerr << "Workload constructor and !nodes" << endl;
            exit(EXIT_FAILURE);
            }
        if (!yamlNodes.IsSequence()) {
            cerr << "Not sequnce in workload type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        
        for (auto yamlNode : yamlNodes) {
            if (!yamlNode.IsMap()) {
                cerr << "Node in workload is not a yaml map" << endl;
                exit(EXIT_FAILURE);
            }
            if (yamlNode["type"].Scalar() == "query" ) {
                nodes[yamlNode["name"].Scalar()] = make_shared<query> (yamlNode);
                // this is an ugly hack for now
                vectornodes.push_back(make_shared<query> (yamlNode));
            }
            else if (yamlNode["type"].Scalar() == "insert") {
                nodes[yamlNode["name"].Scalar()] = make_shared<insert> (yamlNode);
                vectornodes.push_back(make_shared<insert> (yamlNode));
            }
            else
                cerr << "Don't know how to handle workload node with type " << yamlNode["type"] << endl;
        }

        // link the things together
        for (auto mnode : vectornodes) {
            mnode->next = nodes[mnode->nextName];
        }

    }
    
    void workload::execute(mongocxx::client &conn) {
        for (auto mnode : vectornodes) {
            mnode->execute(conn);
        }
    }
}
