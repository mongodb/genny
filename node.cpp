#include "node.hpp"

namespace mwg {

    void node::setNextNode(unordered_map<string,shared_ptr<node>> &nodes) {
        cout << "Setting next node for " << name << ". Next node should be " << nextName << endl;
               nextNode = nodes[nextName];
    }


    void node::executeNode(mongocxx::client &conn) {
        // execute this node
        execute(conn);
        // execute the next node if there is one
        //cout << "just executed " << name << ". NextName is " << nextName << endl;
        if (!nextNode)
            cout << "nextNode is null for some reason" << endl;
        if (name != "Finish" && nextNode) {
            //cout << "About to call nextNode->executeNode" << endl;
            nextNode->executeNode(conn);
        }
    }

}
