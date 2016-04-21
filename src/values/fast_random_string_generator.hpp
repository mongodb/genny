#pragma once

#include <vector>

#include "int_or_value.hpp"
#include "value_generator.hpp"


namespace mwg {

// default alphabet
constexpr char fastAlphaNum[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
constexpr int fastAlphaNumLength = 64;

class FastRandomStringGenerator : public ValueGenerator {
public:
    FastRandomStringGenerator(const YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    IntOrValue length;
};
}
