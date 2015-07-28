#include "finish.hpp"

namespace mwg {
    void finishNode::executeNode(shared_ptr<threadState> myState) {
        // We're done. Just clean up. 
        shared_ptr<node> me = myState.currentNode;
        myState.currentNode = nullptr;
    }
}
