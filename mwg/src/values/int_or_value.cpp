#include "int_or_value.hpp"

#include "value_generator.hpp"
namespace mwg {

IntOrValue::IntOrValue(YAML::Node yamlNode) : myInt(0), myGenerator(nullptr) {
    if (yamlNode.IsScalar()) {
        // Read in just a number
        isInt = true;
        myInt = yamlNode.as<int64_t>();
    } else {
        // use a value generator
        isInt = false;
        myGenerator = makeUniqueValueGenerator(yamlNode);
    }
}
}
