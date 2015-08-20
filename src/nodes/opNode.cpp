#include "opNode.hpp"
#include "operations.hpp"
#include <boost/log/trivial.hpp>

namespace mwg {
opNode::opNode(YAML::Node& ynode) : node(ynode) {
    // Need to parse the operation next.
    auto myop = ynode["op"];
    // would be nice to templatize this whole next part
    // if the operation is embedded in the node, pass that to the op
    if (!myop.IsDefined()) {
        myop = ynode;
        BOOST_LOG_TRIVIAL(debug) << "No myop. Using inline defintion";
    } else
        cout << "Explicit op entry in opNode constructor";
    if (myop["type"].Scalar() == "find")
        op = unique_ptr<operation>(new find(myop));
    else if (myop["type"].Scalar() == "insert_one")
        op = unique_ptr<operation>(new insert_one(myop));
    else if (myop["type"].Scalar() == "insert_many")
        op = unique_ptr<operation>(new insert_many(myop));
    else {
        BOOST_LOG_TRIVIAL(fatal) << "Trying to make operation of type " << myop["type"]
                                 << " is not supported yet in opNode constructor";
        exit(EXIT_FAILURE);
    }
}

void opNode::execute(shared_ptr<threadState> myState) {
    op->execute(myState->conn, *myState);
}
}
