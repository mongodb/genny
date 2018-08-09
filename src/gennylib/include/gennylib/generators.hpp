#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>
#include <random>
#include <unordered_map>

#include "parse_util.hpp"

using view_or_value = bsoncxx::view_or_value<bsoncxx::array::view, bsoncxx::array::value>;

namespace genny {

// Generate a value, such as a random value or access a variable
class ValueGenerator {
public:
    ValueGenerator(const YAML::Node&){};
    virtual ~ValueGenerator(){};
    // Generate a new value.
    virtual bsoncxx::array::value generate(std::mt19937_64&) = 0;
    // Need some helper functions here to get a string, or an int
    virtual int64_t generateInt(std::mt19937_64&);
    virtual double generateDouble(std::mt19937_64&);
    virtual std::string generateString(std::mt19937_64&);
};

const std::set<std::string> getGeneratorTypes();
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node);
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node, std::string);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node, std::string);
std::string valAsString(view_or_value);
int64_t valAsInt(view_or_value);
double valAsDouble(view_or_value);

class UseValueGenerator : public ValueGenerator {
public:
    UseValueGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(std::mt19937_64&) override;

private:
    optional<bsoncxx::array::value> value;
};

// Class to wrap either a plain int64_t, or a value generator that will be called as an int. This
// can be templatized if there are enough variants
class IntOrValue {
public:
    IntOrValue() : myInt(0), myGenerator(nullptr), isInt(true) {}
    IntOrValue(int64_t inInt) : myInt(inInt), myGenerator(nullptr), isInt(true) {}
    IntOrValue(std::unique_ptr<ValueGenerator> generator)
        : myInt(0), myGenerator(std::move(generator)), isInt(false) {}
    IntOrValue(YAML::Node);

    int64_t getInt(std::mt19937_64& state) {
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
    virtual bsoncxx::array::value generate(std::mt19937_64&) override;
    virtual int64_t generateInt(std::mt19937_64&) override;
    virtual std::string generateString(std::mt19937_64&) override;

private:
    GeneratorType generator;
    IntOrValue min;
    IntOrValue max;
    IntOrValue t;                          // for binomial, negative binomial
    std::unique_ptr<ValueGenerator> p;     // for binomial, geometric
    std::unique_ptr<ValueGenerator> mean;  // for poisson
};

// default alphabet
constexpr char fastAlphaNum[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
constexpr int fastAlphaNumLength = 64;

class FastRandomStringGenerator : public ValueGenerator {
public:
    FastRandomStringGenerator(const YAML::Node&);
    virtual bsoncxx::array::value generate(std::mt19937_64&) override;

private:
    IntOrValue length;
};

// default alphabet
static const char alphaNum[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
static const int alphaNumLength = 64;

class RandomStringGenerator : public ValueGenerator {
public:
    RandomStringGenerator(YAML::Node&);
    virtual bsoncxx::array::value generate(std::mt19937_64&) override;

private:
    std::string alphabet;
    IntOrValue length;
};

class document {
public:
    virtual ~document(){};
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc,
                                         std::mt19937_64&) {
        return doc.view();
    };
};

class bsonDocument : public document {
public:
    bsonDocument();
    bsonDocument(const YAML::Node);

    void setDoc(bsoncxx::document::value value) {
        doc = value;
    }
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         std::mt19937_64&) override;

private:
    std::optional<bsoncxx::document::value> doc;
};

class templateDocument : public document {
public:
    templateDocument();
    templateDocument(const YAML::Node);
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&,
                                         std::mt19937_64&) override;

protected:
    // The document to override
    bsonDocument doc;
    unordered_map<string, unique_ptr<ValueGenerator>> override;

private:
    // apply the overides, one level at a time
    void applyOverrideLevel(bsoncxx::builder::stream::document&,
                            bsoncxx::document::view,
                            string,
                            std::mt19937_64&);
};

// parse a YAML Node and make a document of the correct type
unique_ptr<document> makeDoc(const YAML::Node);

}  // namespace genny

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
