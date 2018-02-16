#pragma once

#include "value_generator.hpp"

namespace mwg {

class DateGenerator : public ValueGenerator {
public:
    DateGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
};
}
