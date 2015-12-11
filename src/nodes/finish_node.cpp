#include "finish_node.hpp"

namespace mwg {
void finishNode::executeNode(shared_ptr<threadState> myState) {
    // We're done. Just clean up.
    shared_ptr<node> me = myState->currentNode;
    myState->currentNode = nullptr;
}
std::pair<std::string, std::string> finishNode::generateDotGraph() {
    return (std::pair<std::string, std::string>{"", ""});
}
}
