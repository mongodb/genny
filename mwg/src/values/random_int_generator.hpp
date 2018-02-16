#pragma once

#include "int_or_value.hpp"
#include "value_generator.hpp"

namespace mwg {

enum class GeneratorType {
    UNIFORM,
    BINOMIAL,
    NEGATIVE_BINOMIAL,
    GEOMETRIC,
    POISSON,
};

class RandomIntGenerator : public ValueGenerator {
public:
    RandomIntGenerator(const YAML::Node&);
    virtual bsoncxx::array::value generate(threadState&) override;
    virtual int64_t generateInt(threadState&) override;
    virtual std::string generateString(threadState&) override;

private:
    GeneratorType generator;
    IntOrValue min;
    IntOrValue max;
    IntOrValue t;                          // for binomial, negative binomial
    std::unique_ptr<ValueGenerator> p;     // for binomial, geometric
    std::unique_ptr<ValueGenerator> mean;  // for poisson
};
}
