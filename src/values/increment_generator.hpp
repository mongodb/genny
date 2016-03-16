#pragma once

#include "value_generator.hpp"

namespace mwg {

class IncrementGenerator : public ValueGenerator {
public:
    IncrementGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    std::string variableName;
};
}
