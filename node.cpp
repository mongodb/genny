#include "node.hpp"

namespace mwg {
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
