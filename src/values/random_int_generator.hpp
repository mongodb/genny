#pragma once

#include "value_generator.hpp"

namespace mwg {

class RandomIntGenerator : public ValueGenerator {
public:
    RandomIntGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;
    virtual int64_t generateInt(threadState&) override;
    virtual std::string generateString(threadState&) override;

private:
    int64_t min;
    int64_t max;
};
}
