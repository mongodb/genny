#include "opNode.hpp"
#include "operations.hpp"

namespace mwg { 
    opNode::opNode (YAML::Node &node) {
        // need to set the name
        // these should be made into exceptions
        // should be a map, with type = find
        if (!node) {
            cerr << "Find constructor and !node" << endl;
            exit(EXIT_FAILURE);
        }
        if (!node.IsMap()) {
            cerr << "Not map in find type initializer" << endl;
            exit(EXIT_FAILURE);
        }
        name = node["name"].Scalar();
        nextName = node["next"].Scalar();
        cout << "In opNode constructor. Name: " << name << ", nextName: " << nextName << endl;
        
        // Need to parse the operation next. 
        auto myop = node["op"];
        // would be nice to templatize this whole next part
        // if the operation is embedded in the node, pass that to the op
        if (!myop.IsDefined()) {
            myop = node;
            cout << "No myop. Using inline defintion" << endl;
        }
        else 
            cout << "Explicit op entry in opNode constructor"  << endl;
        if (myop["type"].Scalar() == "find")
            op = unique_ptr<operation>(new find(myop));
        else if (myop["type"].Scalar() == "insert_one")
            op = unique_ptr<operation>(new insert_one(myop));
        else if (myop["type"].Scalar() == "insert_many")
            op = unique_ptr<operation>(new insert_many(myop));
        else {
            cerr << "Trying to make operation of type " << myop["type"] << " is not supported yet in opNode constructor" << endl;
            exit(EXIT_FAILURE);

        }
    }

    void opNode::execute(shared_ptr<threadState> myState) {
        op->execute(myState->conn, myState->rng);
    }

}
