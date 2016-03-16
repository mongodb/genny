#pragma once

#include <memory>

#include <bsoncxx/array/value.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <yaml-cpp/yaml.h>


using bsoncxx::stdx::optional;

namespace mwg {
class threadState;

// Generate a value, such as a random value or access a variable
class ValueGenerator {
public:
    ValueGenerator(YAML::Node&){};
    // Generate a new value.
    virtual bsoncxx::array::value generate(threadState&) = 0;
    // Need some helper functions here to get a string, or an int
    virtual int64_t generateInt(threadState&);
    virtual std::string generateString(threadState&);
};

std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node);
std::string valAsString(bsoncxx::array::value);
int64_t valAsInt(bsoncxx::array::value);
}
