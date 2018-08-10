#ifndef HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
#define HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED

#include <bsoncxx/builder/stream/document.hpp>
#include <optional>
#include <random>
#include <unordered_map>

#include "parse_util.hpp"


namespace genny {

using view_or_value = bsoncxx::view_or_value<bsoncxx::array::view, bsoncxx::array::value>;
// Generate a value, such as a random value or access a variable
class ValueGenerator {
public:
    ValueGenerator(const YAML::Node&, std::mt19937_64& rng) : _rng(rng){};
    virtual ~ValueGenerator(){};
    // Generate a new value.
    virtual bsoncxx::array::value generate() = 0;
    // Need some helper functions here to get a string, or an int
    virtual int64_t generateInt();
    virtual double generateDouble();
    virtual std::string generateString();
    // Class to wrap either a plain int64_t, or a value generator that will be called as an int.

protected:
    std::mt19937_64& _rng;
};

const std::set<std::string> getGeneratorTypes();
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node, std::mt19937_64&);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node, std::mt19937_64&);
std::unique_ptr<ValueGenerator> makeUniqueValueGenerator(YAML::Node, std::string, std::mt19937_64&);
std::shared_ptr<ValueGenerator> makeSharedValueGenerator(YAML::Node, std::string, std::mt19937_64&);
std::string valAsString(view_or_value);
int64_t valAsInt(view_or_value);
double valAsDouble(view_or_value);

class UseValueGenerator : public ValueGenerator {
public:
    UseValueGenerator(YAML::Node&, std::mt19937_64&);
    virtual bsoncxx::array::value generate() override;

private:
    optional<bsoncxx::array::value> value;
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
    IntOrValue(YAML::Node, std::mt19937_64&);

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
enum class GeneratorType {
    UNIFORM,
    BINOMIAL,
    NEGATIVE_BINOMIAL,
    GEOMETRIC,
    POISSON,
};

class RandomIntGenerator : public ValueGenerator {
public:
    RandomIntGenerator(const YAML::Node&, std::mt19937_64&);
    virtual bsoncxx::array::value generate() override;
    virtual int64_t generateInt() override;
    virtual std::string generateString() override;

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
    FastRandomStringGenerator(const YAML::Node&, std::mt19937_64&);
    virtual bsoncxx::array::value generate() override;

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
    RandomStringGenerator(YAML::Node&, std::mt19937_64&);
    virtual bsoncxx::array::value generate() override;

private:
    std::string alphabet;
    IntOrValue length;
};

class Document {
public:
    virtual ~Document(){};
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document& doc) {
        return doc.view();
    };
};

class BsonDocument : public Document {
public:
    BsonDocument();
    BsonDocument(const YAML::Node);

    void setDoc(bsoncxx::document::value value) {
        doc = value;
    }
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&) override;

private:
    std::optional<bsoncxx::document::value> doc;
};

class TemplateDocument : public Document {
public:
    TemplateDocument();
    TemplateDocument(const YAML::Node, std::mt19937_64&);
    virtual bsoncxx::document::view view(bsoncxx::builder::stream::document&) override;

protected:
    // The document to override
    BsonDocument doc;
    unordered_map<string, unique_ptr<ValueGenerator>> override;

private:
    // apply the overides, one level at a time
    void applyOverrideLevel(bsoncxx::builder::stream::document&, bsoncxx::document::view, string);
};

// parse a YAML Node and make a document of the correct type
unique_ptr<Document> makeDoc(const YAML::Node, std::mt19937_64&);

}  // namespace genny

#endif  // HEADER_E6E05F14_BE21_4A9B_822D_FFD669CFB1B4_INCLUDED
