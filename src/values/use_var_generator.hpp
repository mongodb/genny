#pragma once

#include "value_generator.hpp"

namespace mwg {

class UseVarGenerator : public ValueGenerator {
public:
    UseVarGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    std::string variableName;
};
}
