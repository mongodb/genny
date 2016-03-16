#pragma once

#include <vector>

#include "value_generator.hpp"


namespace mwg {

// default alphabet
static const char alphaNum[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
static const int alphaNumLength = 64;

class RandomStringGenerator : public ValueGenerator {
public:
    RandomStringGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;

private:
    std::string alphabet;
    int64_t length;
};
}
