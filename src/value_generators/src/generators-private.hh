#ifndef HEADER_48C0CEE4_FC42_4B98_AABE_5980BF83A174
#define HEADER_48C0CEE4_FC42_4B98_AABE_5980BF83A174

#include "parser.hh"

#include <value_generators/value_generators.hpp>
#include <value_generators/DefaultRandom.hpp>

namespace genny::value_generators {

using view_or_value = bsoncxx::view_or_value<bsoncxx::array::view, bsoncxx::array::value>;

// Generate a value, such as a random value or access a variable
class ValueGenerator {
public:
    ValueGenerator(const YAML::Node&, genny::DefaultRandom& rng) : _rng(rng){};
    virtual ~ValueGenerator(){};
    // Generate a new value.
    virtual bsoncxx::array::value generate() = 0;
    // Need some helper functions here to get a string, or an int
    virtual int64_t generateInt();
    virtual double generateDouble();
    virtual std::string generateString();
    // Class to wrap either a plain int64_t, or a value generator that will be called as an int.

protected:
    genny::DefaultRandom& _rng;
};

std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node, genny::DefaultRandom&);
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node, std::string, genny::DefaultRandom&);
std::string valAsString(view_or_value);
int64_t valAsInt(view_or_value);
double valAsDouble(view_or_value);

class UseValueGenerator : public ValueGenerator {
public:
    UseValueGenerator(YAML::Node&, genny::DefaultRandom&);
    virtual bsoncxx::array::value generate() override;

private:
    std::optional<bsoncxx::array::value> value;
};

// Class to wrap either a plain int64_t, or a value generator that will be called as an int. This
// can be templatized if there are enough variants
// This can be templatized if there are enough variants
class IntOrValue {
public:
    IntOrValue() : myInt(0), myGenerator(nullptr), isInt(true) {}
    IntOrValue(int64_t inInt) : myInt(inInt), myGenerator(nullptr), isInt(true) {}
    IntOrValue(std::unique_ptr<ValueGenerator> generator)
        : myInt(0), myGenerator(std::move(generator)), isInt(false) {}
    IntOrValue(YAML::Node, genny::DefaultRandom&);

    int64_t getInt() {
        if (isInt) {
            return (myInt);
        } else
            return myGenerator->generateInt();
    }


private:
    int64_t myInt;
    std::unique_ptr<ValueGenerator> myGenerator;
    bool isInt;
};
class RandomIntGenerator : public ValueGenerator {
public:
    RandomIntGenerator(const YAML::Node&, genny::DefaultRandom&);
    virtual bsoncxx::array::value generate() override;
    virtual int64_t generateInt() override;
    virtual std::string generateString() override;

private:
    enum class GeneratorType {
        UNIFORM,
        BINOMIAL,
        NEGATIVE_BINOMIAL,
        GEOMETRIC,
        POISSON,
    };

    GeneratorType generator;
    IntOrValue min;
    IntOrValue max;
    IntOrValue t;  // for binomial, negative binomial
    double p;     // for binomial, geometric
    double mean;  // for poisson
};

/**
 * Generates random strings.
 * @private
 */
class FastRandomStringGenerator : public ValueGenerator {
public:
    FastRandomStringGenerator(const YAML::Node&, genny::DefaultRandom&);
    bsoncxx::array::value generate() override;

private:
    // default alphabet
    static constexpr char fastAlphaNum[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    static constexpr int fastAlphaNumLength = 64;
    IntOrValue length;
};

/**
 * A fast random string generator.
 *
 * @private
 */
class RandomStringGenerator : public ValueGenerator {
public:
    RandomStringGenerator(YAML::Node&, genny::DefaultRandom&);
    bsoncxx::array::value generate() override;

private:
    // default alphabet
    static constexpr char alphaNum[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    static constexpr int alphaNumLength = 64;
    std::string alphabet;
    IntOrValue length;
};

class BsonDocument : public DocumentGenerator {
public:
    BsonDocument();
    explicit BsonDocument(YAML::Node);

    void setDoc(bsoncxx::document::value value) {
        doc = value;
    }

    bsoncxx::document::view view(bsoncxx::builder::stream::document&) override;

private:
    std::optional<bsoncxx::document::value> doc;
};

class TemplateDocument : public DocumentGenerator {
public:
    TemplateDocument(YAML::Node, genny::DefaultRandom&);
    bsoncxx::document::view view(bsoncxx::builder::stream::document&) override;

protected:
    // The document to override
    BsonDocument doc;
    std::unordered_map<std::string, std::unique_ptr<ValueGenerator>> override;

private:
    // apply the overrides, one level at a time
    void applyOverrideLevel(bsoncxx::builder::stream::document&,
                            bsoncxx::document::view,
                            std::string);
};
}  // namespace genny::value_generators


#endif  // HEADER_48C0CEE4_FC42_4B98_AABE_5980BF83A174
