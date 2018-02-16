#pragma once

#include "value_generator.hpp"

namespace mwg {

class UseValueGenerator : public ValueGenerator {
public:
    UseValueGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    optional<bsoncxx::array::value> value;
};
}
