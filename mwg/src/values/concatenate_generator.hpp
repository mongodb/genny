#pragma once

#include <vector>

#include "value_generator.hpp"


namespace mwg {

class ConcatenateGenerator : public ValueGenerator {
public:
    ConcatenateGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    // Vector of generators to concatenate
    // Probably should be StringOrGenerator
    std::vector<std::unique_ptr<ValueGenerator>> generators;
};
}
