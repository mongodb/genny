#pragma once

#include <memory>
#include <unordered_map>

#include "overrideDocument.hpp"

using namespace std;

namespace mwg {
class templateDocument : public overrideDocument {
public:
    templateDocument() : overrideDocument(){};
    templateDocument(YAML::Node&);
};
}
