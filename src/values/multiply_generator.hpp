#pragma once

#include "value_generator.hpp"

namespace mwg {

class MultiplyGenerator : public ValueGenerator {
public:
    MultiplyGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    // this should evolve to be two value generators, or possibly a list of generators
    std::string variableName;
    uint64_t factor;
};
}
