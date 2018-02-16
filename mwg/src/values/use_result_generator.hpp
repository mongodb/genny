#pragma once

#include "value_generator.hpp"

namespace mwg {

class UseResultGenerator : public ValueGenerator {
public:
    UseResultGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
};
}
