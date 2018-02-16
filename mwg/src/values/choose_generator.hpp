#pragma once

#include "value_generator.hpp"

namespace mwg {

class ChooseGenerator : public ValueGenerator {
public:
    ChooseGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    // probably should be objects. Starting with strings
    std::vector<bsoncxx::array::value> choices;
};
}
