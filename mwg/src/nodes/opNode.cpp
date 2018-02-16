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
        BOOST_LOG_TRIVIAL(debug) << "Explicit op entry in opNode constructor";
    if (myop["type"].Scalar() == "find")
        op = unique_ptr<operation>(new find(myop));
    else if (myop["type"].Scalar() == "count")
        op = unique_ptr<operation>(new count(myop));
    else if (myop["type"].Scalar() == "insert_one")
        op = unique_ptr<operation>(new insert_one(myop));
    else if (myop["type"].Scalar() == "insert_many")
        op = unique_ptr<operation>(new insert_many(myop));
    else if (myop["type"].Scalar() == "delete_many")
        op = unique_ptr<operation>(new delete_many(myop));
    else if (myop["type"].Scalar() == "delete_one")
        op = unique_ptr<operation>(new delete_one(myop));
    else if (myop["type"].Scalar() == "create_index")
        op = unique_ptr<operation>(new create_index(myop));
    else if (myop["type"].Scalar() == "distinct")
        op = unique_ptr<operation>(new distinct(myop));
    else if (myop["type"].Scalar() == "drop")
        op = unique_ptr<operation>(new drop(myop));
    else if (myop["type"].Scalar() == "find_one")
        op = unique_ptr<operation>(new find_one(myop));
    else if (myop["type"].Scalar() == "find_one_and_update")
        op = unique_ptr<operation>(new find_one_and_update(myop));
    else if (myop["type"].Scalar() == "find_one_and_replace")
        op = unique_ptr<operation>(new find_one_and_replace(myop));
    else if (myop["type"].Scalar() == "find_one_and_delete")
        op = unique_ptr<operation>(new find_one_and_delete(myop));
    else if (myop["type"].Scalar() == "replace_one")
        op = unique_ptr<operation>(new replace_one(myop));
    else if (myop["type"].Scalar() == "list_indexes")
        op = unique_ptr<operation>(new list_indexes(myop));
    else if (myop["type"].Scalar() == "read_preference")
        op = unique_ptr<operation>(new read_preference(myop));
    else if (myop["type"].Scalar() == "write_concern")
        op = unique_ptr<operation>(new write_concern(myop));
    else if (myop["type"].Scalar() == "name")
        op = unique_ptr<operation>(new class name(myop));
    else if (myop["type"].Scalar() == "update_one")
        op = unique_ptr<operation>(new update_one(myop));
    else if (myop["type"].Scalar() == "update_many")
        op = unique_ptr<operation>(new update_many(myop));
    // else if (myop["type"].Scalar() == "create_collection") // c++ driver not supporting this yet
    //     op = unique_ptr<operation>(new create_collection(myop));
    else if (myop["type"].Scalar() == "command")
        op = unique_ptr<operation>(new command(myop));
    else if (myop["type"].Scalar() == "set_variable")
        op = unique_ptr<operation>(new set_variable(myop));
    else if (myop["type"].Scalar() == "noop")
        op = unique_ptr<operation>(new noop(myop));
    else {
        BOOST_LOG_TRIVIAL(fatal) << "Trying to make operation of type " << myop["type"]
                                 << " is not supported yet in opNode constructor";
        exit(EXIT_FAILURE);
    }
}

void opNode::execute(shared_ptr<threadState> myState) {
    // BOOST_LOG_TRIVIAL(trace) << "opNode::execute";
    op->execute(myState->conn, *myState);
}
}
