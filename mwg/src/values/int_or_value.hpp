#pragma once

#include <memory>
#include <yaml-cpp/yaml.h>

#include "value_generator.hpp"

namespace mwg {
class threadState;

// Class to wrap either a plain int64_t, or a value generator that will be called as an int. This
// can be templatized if there are enough variants
class IntOrValue {
public:
    IntOrValue() : myInt(0), myGenerator(nullptr), isInt(true) {}
    IntOrValue(int64_t inInt) : myInt(inInt), myGenerator(nullptr), isInt(true) {}
    IntOrValue(std::unique_ptr<ValueGenerator> generator)
        : myInt(0), myGenerator(std::move(generator)), isInt(false) {}
    IntOrValue(YAML::Node);

    int64_t getInt(threadState& state) {
        if (isInt) {
            return (myInt);
        } else
            return myGenerator->generateInt(state);
    }


private:
    int64_t myInt;
    std::unique_ptr<ValueGenerator> myGenerator;
    bool isInt;
};
}
