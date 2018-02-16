#include "random_int_generator.hpp"

#include <boost/log/trivial.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <random>

#include "threadState.hpp"
#include "workload.hpp"


using bsoncxx::builder::stream::finalize;

namespace mwg {
RandomIntGenerator::RandomIntGenerator(const YAML::Node& node)
    : ValueGenerator(node), generator(GeneratorType::UNIFORM), min(0), max(100), t(10) {
    // It's okay to have a scalar for the templating. Just use defaults
    if (node.IsMap()) {
        if (auto distributionNode = node["distribution"]) {
            auto distributionString = distributionNode.Scalar();
            if (distributionString == "uniform")
                generator = GeneratorType::UNIFORM;
            else if (distributionString == "binomial")
                generator = GeneratorType::BINOMIAL;
            else if (distributionString == "negative_binomial")
                generator = GeneratorType::NEGATIVE_BINOMIAL;
            else if (distributionString == "geometric")
                generator = GeneratorType::GEOMETRIC;
            else if (distributionString == "poisson")
                generator = GeneratorType::POISSON;
            else {
                BOOST_LOG_TRIVIAL(fatal)
                    << "In RandomIntGenerator and have unknown distribution type "
                    << distributionString;
                exit(EXIT_FAILURE);
            }
        }
        // now read in parameters based on the distribution type
        switch (generator) {
            case GeneratorType::UNIFORM:
                if (auto minimum = node["min"]) {
                    min = IntOrValue(minimum);
                }
                if (auto maximum = node["max"]) {
                    max = IntOrValue(maximum);
                }
                break;
            case GeneratorType::BINOMIAL:
                if (auto trials = node["t"])
                    t = IntOrValue(trials);
                else
                    BOOST_LOG_TRIVIAL(warning)
                        << "Binomial distribution in random int, but no t parameter";
                if (auto probability = node["p"])
                    p = makeUniqueValueGenerator(probability);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Binomial distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            case GeneratorType::NEGATIVE_BINOMIAL:
                if (auto kval = node["k"])
                    t = IntOrValue(kval);
                else
                    BOOST_LOG_TRIVIAL(warning)
                        << "Negative binomial distribution in random int, not no k parameter";
                if (auto probability = node["p"])
                    p = makeUniqueValueGenerator(probability);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Binomial distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            case GeneratorType::GEOMETRIC:
                if (auto probability = node["p"])
                    p = makeUniqueValueGenerator(probability);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Geometric distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            case GeneratorType::POISSON:
                if (auto meannode = node["mean"])
                    mean = makeUniqueValueGenerator(meannode);
                else {
                    BOOST_LOG_TRIVIAL(fatal)
                        << "Geometric distribution in random int, but no p parameter";
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                BOOST_LOG_TRIVIAL(fatal)
                    << "Unknown generator type in RandomIntGenerator in switch statement";
                exit(EXIT_FAILURE);
                break;
        }
    }
}

int64_t RandomIntGenerator::generateInt(threadState& state) {
    switch (generator) {
        case GeneratorType::UNIFORM: {
            uniform_int_distribution<int64_t> distribution(min.getInt(state), max.getInt(state));
            return (distribution(state.rng));
        } break;
        case GeneratorType::BINOMIAL: {
            binomial_distribution<int64_t> distribution(t.getInt(state), p->generateDouble(state));
            return (distribution(state.rng));
        } break;
        case GeneratorType::NEGATIVE_BINOMIAL: {
            negative_binomial_distribution<int64_t> distribution(t.getInt(state),
                                                                 p->generateDouble(state));
            return (distribution(state.rng));
        } break;
        case GeneratorType::GEOMETRIC: {
            geometric_distribution<int64_t> distribution(p->generateDouble(state));
            return (distribution(state.rng));
        } break;
        case GeneratorType::POISSON: {
            poisson_distribution<int64_t> distribution(mean->generateDouble(state));
            return (distribution(state.rng));
        } break;
        default:
            BOOST_LOG_TRIVIAL(fatal)
                << "Unknown generator type in RandomIntGenerator in switch statement";
            exit(EXIT_FAILURE);
            break;
    }
    BOOST_LOG_TRIVIAL(fatal)
        << "Reached end of RandomIntGenerator::generateInt. Should have returned earlier";
    exit(EXIT_FAILURE);
}
std::string RandomIntGenerator::generateString(threadState& state) {
    return (std::to_string(generateInt(state)));
}
bsoncxx::array::value RandomIntGenerator::generate(threadState& state) {
    return (bsoncxx::builder::stream::array{} << generateInt(state)
                                              << bsoncxx::builder::stream::finalize);
}
}
