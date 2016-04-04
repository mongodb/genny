#pragma once

#include <memory>
#include <vector>

#include "value_generator.hpp"

namespace mwg {

class AddGenerator : public ValueGenerator {
public:
    AddGenerator(YAML::Node&);
    bsoncxx::array::value generate(threadState&) override;
    int64_t generateInt(threadState&) override;
    double generateDouble(threadState&) override;
    std::string generateString(threadState&) override;

private:
    std::vector<std::unique_ptr<ValueGenerator>> addends;
};
}
