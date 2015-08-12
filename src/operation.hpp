#include "yaml-cpp/yaml.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "../threadState.hpp"

#pragma once
using namespace std;

namespace mwg {

class operation {
public:
    operation(){};
    virtual ~operation() = default;
    operation(const operation&) = default;
    operation(operation&&) = default;
    // Execute the node
    virtual void execute(mongocxx::client&, threadState&) = 0;
};
}
