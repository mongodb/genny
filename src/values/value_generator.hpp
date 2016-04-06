#pragma once

#include <memory>

#include <bsoncxx/array/value.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <bsoncxx/view_or_value.hpp>
#include <yaml-cpp/yaml.h>


using bsoncxx::stdx::optional;
using view_or_value = bsoncxx::view_or_value<bsoncxx::array::view, bsoncxx::array::value>;

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
    virtual double generateDouble(threadState&);
    virtual std::string generateString(threadState&);
};

const std::set<std::string> getGeneratorTypes();
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node);
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node, std::string);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node, std::string);
std::string valAsString(view_or_value);
int64_t valAsInt(view_or_value);
double valAsDouble(view_or_value);
}
