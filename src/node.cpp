#include "node.hpp"

namespace mwg {

    void node::setNextNode(unordered_map<string,shared_ptr<node>> &nodes) {
        cout << "Setting next node for " << name << ". Next node should be " << nextName << endl;
               nextNode = nodes[nextName];
    }


    void node::executeNode(shared_ptr<threadState> myState) {
        // execute this node
        execute(myState);
        // execute the next node if there is one
        //cout << "just executed " << name << ". NextName is " << nextName << endl;
        auto next = nextNode.lock();
        if (!next)
            cout << "nextNode is null for some reason" << endl;
        if (name != "Finish" && next && !stopped) {
            //cout << "About to call nextNode->executeNode" << endl;
            // update currentNode in the state. Protect the reference while executing
            shared_ptr<node> me = myState->currentNode;
            myState->currentNode = next;
            next->executeNode(myState);
        }
    }

}
