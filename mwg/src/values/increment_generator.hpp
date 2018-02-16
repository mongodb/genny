// Copyright 2016 MongoDB

#pragma once

#include <string>

#include "int_or_value.hpp"
#include "value_generator.hpp"

namespace mwg {

class IncrementGenerator : public ValueGenerator {
public:
    explicit IncrementGenerator(YAML::Node&);
    bsoncxx::array::value generate(threadState&) override;

private:
    std::string variableName;
    IntOrValue minimum;
    IntOrValue maximum;
    IntOrValue increment;
};
}  // namespace mwg
