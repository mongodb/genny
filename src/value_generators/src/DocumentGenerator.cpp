// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <mutex>
#include <value_generators/DocumentGenerator.hpp>
#include <value_generators/FrequencyMap.hpp>

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <boost/algorithm/string/join.hpp>
#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/math/special_functions/pow.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/decimal128.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types/bson_value/view.hpp>

namespace {

template <class IntType = int64_t>
class zipfian_distribution {
public:
    explicit zipfian_distribution(double alpha, IntType n)
        : _alpha{alpha}, _n{n}, _c{calculateNormalizationConstant(n, alpha)} {}

    template <class URNG>
    IntType operator()(URNG& urng) {
        return generate(urng);
    }

private:
    // Shape parameter for the distribution.
    double _alpha;
    // Normalization constant for the distribution.
    double _c;
    // Number of distinct elements in the distribution.
    IntType _n;

    // This implementation is based on https://cse.usf.edu/~kchriste/tools/genzipf.c,
    // it uses the inverse transform sampling method for generating random numbers.
    // This method is not the most accurate and efficient for heavy tailed distributions,
    // but is sufficient for our current purposes as we're not doing any numerical analysis.
    template <class URNG>
    IntType generate(URNG& urng) {
        boost::random::uniform_01<> _uniform01{};
        double randomNumber = 1.0 - _uniform01(urng);
        double sum = 0;

        for (IntType i = 1; i <= _n; ++i) {
            // std::pow might be a point for optimization, although fast calculation
            // of powers might reduce accuracy, so this is TBD.
            // Another point for optimization is to hold these cumulative sum values
            // in a precomputed array since they are the same for generate() calls.
            sum += 1.0 / std::pow(i, _alpha);
            if (sum >= randomNumber * _c) {
                return i;
            }
        }
    }

    double calculateNormalizationConstant(IntType n, double alpha) {
        double constant = 0;
        for (int64_t i = 1; i <= n; ++i) {
            constant += 1.0 / std::pow(i, alpha);
        }
        return constant;
    }
};
}  // namespace

namespace {
using bsoncxx::oid;

class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
    virtual void append(bsoncxx::builder::basic::array& builder) = 0;

protected:
    // Custom hasher to enable hashing for BSON types.
    struct SetHasher {
        template <typename T>
        size_t operator()(const T& v) const {
            return std::hash<T>{}(v);
        }

        size_t operator()(const bsoncxx::array::value& value) const {
            // Hash the view as a string.
            const auto v = value.view();
            std::string_view sv{reinterpret_cast<const char*>(v.data()), v.length()};
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const bsoncxx::document::value& value) const {
            // Hash the view as a string.
            const auto v = value.view();
            std::string_view sv{reinterpret_cast<const char*>(v.data()), v.length()};
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const bsoncxx::types::b_date& value) const {
            return std::hash<int64_t>{}(value.to_int64());
        }
    };

    // Custom equals comparator to enable comparison for certain BSON types.
    struct SetKeyEq {
        template <typename T>
        bool operator()(const T& lhs, const T& rhs) const {
            return lhs == rhs;
        }

        bool operator()(const bsoncxx::array::value& lhs, const bsoncxx::array::value& rhs) const {
            return lhs.view() == rhs.view();
        }
    };
};

using UniqueAppendable = std::unique_ptr<Appendable>;
}  // namespace

namespace genny {

template <class T>
class Generator : public Appendable {
public:
    ~Generator() override = default;
    virtual T evaluate() = 0;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, this->evaluate()));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(this->evaluate());
    }
};
}  // namespace genny

namespace {
using namespace genny;
const static boost::posix_time::ptime epoch{boost::gregorian::date(1970, 1, 1)};

template <typename T>
class ConstantAppender : public Generator<T> {
public:
    explicit ConstantAppender(T value) : _value{value} {}
    explicit ConstantAppender() : _value{} {}
    T evaluate() override {
        return _value;
    }

protected:
    T _value;
};
}  // namespace

namespace genny {
class DocumentGenerator::Impl : public Generator<bsoncxx::document::value> {
public:
    using Entries = std::vector<std::pair<std::string, UniqueAppendable>>;

    explicit Impl(Entries entries) : _entries{std::move(entries)} {}

    bsoncxx::document::value evaluate() override {
        bsoncxx::builder::basic::document builder;
        for (auto&& [k, app] : _entries) {
            app->append(k, builder);
        }
        return builder.extract();
    }

private:
    Entries _entries;
};
}  // namespace genny

namespace {

/**
 * Extract a required key from a node or barf if not there
 * @param node
 *   haystack to look in
 * @param key
 *   needle key to look for
 * @param src
 *   useful source location included in the exception message
 * @return
 *   `node[key]` if exists else throw with a meaningful error message
 */
const Node& extract(const Node& node, const std::string& key, const std::string& src) {
    auto& out = node[key];
    if (!out) {
        std::stringstream ex;
        ex << "Missing '" << key << "' for '" << src << "' in input " << node;
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(ex.str()));
    }
    return out;
}

/**
 * @param node
 *   a "sub"-structure e.g. should get `{v}` in `{a:{v}`
 * @return
 *   the single ^-prefixed key if there is exactly one
 *   ^-prefixed key. If there is a ^-prefixed keys
 *   *and* any additional keys then barf.
 */
std::optional<std::string> getMetaKey(const Node& node) {
    size_t foundKeys = 0;
    std::optional<std::string> out = std::nullopt;
    for (const auto&& [k, v] : node) {
        ++foundKeys;
        auto key = k.toString();
        if (!key.empty() && key[0] == '^') {
            out = key;
        }
        if (foundKeys > 1 && out) {
            std::stringstream msg;
            msg << "Found multiple meta-keys on node at path: " << node.path();
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
    }
    return out;
}

/** Default alphabet for string generators */
static const std::string kDefaultAlphabet = std::string{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/"};

// Useful typedefs

template <typename O>
using Parser = std::function<O(const Node&, GeneratorArgs)>;

const static boost::posix_time::ptime max_date{boost::gregorian::date(2150, 1, 1)};

// Pre-declaring all at once
// Documentation is at the implementations-site.
int64_t parseStringToMillis(const std::string& datetime);
UniqueGenerator<int64_t> intGenerator(const Node& node, GeneratorArgs generatorArgs);
UniqueGenerator<int64_t> int64GeneratorBasedOnDistribution(const Node& node,
                                                           GeneratorArgs generatorArgs);
UniqueGenerator<double> doubleGenerator(const Node& node, GeneratorArgs generatorArgs);
UniqueGenerator<double> doubleGeneratorBasedOnDistribution(const Node& node,
                                                           GeneratorArgs generatorArgs);
UniqueGenerator<int32_t> int32Generator(const Node& node, GeneratorArgs generatorArgs);
UniqueGenerator<std::string> stringGenerator(const Node& node, GeneratorArgs generatorArgs);
UniqueGenerator<int64_t> dateGenerator(const Node& node,
                                       GeneratorArgs generatorArgs,
                                       const boost::posix_time::ptime& defaultTime = epoch);
template <bool Verbatim, typename Out>
Out valueGenerator(const Node& node,
                   GeneratorArgs generatorArgs,
                   const std::map<std::string, Parser<Out>>& parsers);

template <bool Verbatim>
std::unique_ptr<DocumentGenerator::Impl> documentGenerator(const Node& node,
                                                           GeneratorArgs generatorArgs);

template <bool Verbatim>
UniqueGenerator<bsoncxx::array::value> literalArrayGenerator(const Node& node,
                                                             GeneratorArgs generatorArgs);

UniqueGenerator<bsoncxx::array::value> bsonArrayGenerator(const Node& node,
                                                          GeneratorArgs generatorArgs);

template <typename Distribution,
          const char* diststring,
          const char* parameter1name,
          const char* parameter2name>
class DoubleGenerator2Parameter : public Generator<double> {
public:
    DoubleGenerator2Parameter(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _actorId{generatorArgs.actorId},
          _parameter1Gen{doubleGenerator(extract(node, parameter1name, diststring), generatorArgs)},
          _parameter2Gen{
              doubleGenerator(extract(node, parameter2name, diststring), generatorArgs)} {}

    double evaluate() override {
        auto parameter1 = _parameter1Gen->evaluate();
        auto parameter2 = _parameter2Gen->evaluate();
        auto dist = Distribution{parameter1, parameter2};
        return dist(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _actorId;
    UniqueGenerator<double> _parameter1Gen;
    UniqueGenerator<double> _parameter2Gen;
};

template <typename Distribution, const char* diststring, const char* parameter1name>
class DoubleGenerator1Parameter : public Generator<double> {
public:
    DoubleGenerator1Parameter(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _actorId{generatorArgs.actorId},
          _parameter1Gen{
              doubleGenerator(extract(node, parameter1name, diststring), generatorArgs)} {}

    double evaluate() override {
        auto parameter1 = _parameter1Gen->evaluate();
        auto dist = Distribution{parameter1};
        return dist(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _actorId;
    UniqueGenerator<double> _parameter1Gen;
};

// Constant strings for arguments for templates
static const char astr[] = "a";
static const char bstr[] = "b";
static const char kstr[] = "k";
static const char mstr[] = "m";
static const char nstr[] = "n";
static const char sstr[] = "s";
static const char minstr[] = "min";
static const char maxstr[] = "max";
static const char stepstr[] = "step";
static const char startstr[] = "start";
static const char multiplierstr[] = "multiplier";
static const char alphastr[] = "alpha";
static const char betastr[] = "beta";
static const char lambdastr[] = "lambda";
static const char meanstr[] = "mean";
static const char medianstr[] = "median";
static const char sigmastr[] = "sigma";

// constant strings for distribution names in templates
static const char uniformstr[] = "uniform";
static const char exponentialstr[] = "exponential";
static const char gammastr[] = "gamma";
static const char weibullstr[] = "weibull";
static const char extremestr[] = "extreme_value";
static const char laplacestr[] = "laplace";
static const char normalstr[] = "normal";
static const char lognormalstr[] = "lognormal";
static const char chisquaredstr[] = "chi_squared";
static const char noncentralchisquaredstr[] = "non_central_chi_squared";
static const char cauchystr[] = "cauchy";
static const char fisherfstr[] = "fisher_f";
static const char studenttstr[] = "student_t";

using UniformDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::uniform_real_distribution<double>,
                              uniformstr,
                              minstr,
                              maxstr>;
using ExponentialDoubleGenerator =
    DoubleGenerator1Parameter<boost::random::exponential_distribution<double>,
                              exponentialstr,
                              lambdastr>;
using GammaDoubleGenerator = DoubleGenerator2Parameter<boost::random::gamma_distribution<double>,
                                                       gammastr,
                                                       alphastr,
                                                       betastr>;
using WeibullDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::weibull_distribution<double>, weibullstr, astr, bstr>;
using ExtremeValueDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::extreme_value_distribution<double>,
                              extremestr,
                              astr,
                              bstr>;
using BetaDoubleGenerator =
    DoubleGenerator1Parameter<boost::random::beta_distribution<double>, betastr, alphastr>;
using LaplaceDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::laplace_distribution<double>,
                              laplacestr,
                              meanstr,
                              betastr>;
using NormalDoubleGenerator = DoubleGenerator2Parameter<boost::random::normal_distribution<double>,
                                                        normalstr,
                                                        meanstr,
                                                        sigmastr>;
using LognormalDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::lognormal_distribution<double>,
                              lognormalstr,
                              mstr,
                              sstr>;
using ChiSquaredDoubleGenerator =
    DoubleGenerator1Parameter<boost::random::chi_squared_distribution<double>, chisquaredstr, nstr>;
using CauchyDoubleGenerator = DoubleGenerator2Parameter<boost::random::cauchy_distribution<double>,
                                                        cauchystr,
                                                        medianstr,
                                                        sigmastr>;

// The NonCentralChiSquaredDoubleGenerator, FisherF, and StudentT distributions
// behave differently on different platforms.
// As such there is no automated testing of them.
using NonCentralChiSquaredDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::non_central_chi_squared_distribution<double>,
                              noncentralchisquaredstr,
                              kstr,
                              lambdastr>;
using FisherFDoubleGenerator =
    DoubleGenerator2Parameter<boost::random::cauchy_distribution<double>, fisherfstr, mstr, nstr>;
using StudentTDoubleGenerator =
    DoubleGenerator1Parameter<boost::random::student_t_distribution<double>, studenttstr, nstr>;

/** `{^RandomInt:{distribution:uniform ...}}` */
class UniformInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{min:<int>, max:<int>}` */
    UniformInt64Generator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _minGen{intGenerator(extract(node, "min", "uniform"), generatorArgs)},
          _maxGen{intGenerator(extract(node, "max", "uniform"), generatorArgs)} {}

    int64_t evaluate() override {
        auto min = _minGen->evaluate();
        auto max = _maxGen->evaluate();
        auto distribution = boost::random::uniform_int_distribution<int64_t>{min, max};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _id;
    UniqueGenerator<int64_t> _minGen;
    UniqueGenerator<int64_t> _maxGen;
};

/** `{^RandomInt:{distribution:binomial ...}}` */
class BinomialInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{t:<int>, p:double}` */
    BinomialInt64Generator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _tGen{intGenerator(extract(node, "t", "binomial"), generatorArgs)},
          _p{extract(node, "p", "binomial").to<double>()} {}

    int64_t evaluate() override {
        auto distribution = boost::random::binomial_distribution<int64_t>{_tGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _id;
    double _p;
    UniqueGenerator<int64_t> _tGen;
};

/** `{^RandomInt:{distribution:negative_binomial ...}}` */
class NegativeBinomialInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{k:<int>, p:double}` */
    NegativeBinomialInt64Generator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _kGen{intGenerator(extract(node, "k", "negative_binomial"), generatorArgs)},
          _p{extract(node, "p", "negative_binomial").to<double>()} {}

    int64_t evaluate() override {
        auto distribution =
            boost::random::negative_binomial_distribution<int64_t>{_kGen->evaluate(), _p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _id;
    double _p;
    UniqueGenerator<int64_t> _kGen;
};

/** `{^RandomInt:{distribution:poisson...}}` */
class PoissonInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{mean:double}` */
    PoissonInt64Generator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _mean{extract(node, "mean", "poisson").to<double>()} {}

    int64_t evaluate() override {
        auto distribution = boost::random::poisson_distribution<int64_t>{_mean};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _id;
    double _mean;
};

/** `{^RandomInt:{distribution:geometric...}}` */
class GeometricInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{mean:double}` */
    GeometricInt64Generator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _p{extract(node, "p", "geometric").to<double>()} {}

    int64_t evaluate() override {
        auto distribution = boost::random::geometric_distribution<int64_t>{_p};
        return distribution(_rng);
    }

private:
    DefaultRandom& _rng;
    ActorId _id;
    const double _p;
};

/** `{^RandomInt:{distribution:zipfian...}}` */
class ZipfianInt64Generator : public Generator<int64_t> {
public:
    /** @param node `{alpha:double, n:<int>}` */
    ZipfianInt64Generator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _distribution{extract(node, "alpha", "zipfian").to<double>(),
                        intGenerator(extract(node, "n", "zipfian"), generatorArgs)->evaluate()} {}

    int64_t evaluate() override {
        return _distribution(_rng);
    }

private:
    ActorId _id;
    DefaultRandom& _rng;
    zipfian_distribution<int64_t> _distribution;
};

// This generator allows choosing any valid generator, incuding documents. As such it cannot be used
// by JoinGenerator today. See ChooseStringGenerator.
class ChooseGenerator : public Appendable {
public:
    // constructor defined at bottom of the file to use other symbol
    ChooseGenerator(const Node& node, GeneratorArgs generatorArgs);
    Appendable& choose() {
        if (_deterministic) {
            ++_elemNumber;
            return *_choices[_elemNumber % _choices.size()];
        }
        // Pick a random number between 0 and sum(weights)
        // Pick value based on that.
        auto distribution = boost::random::discrete_distribution(_weights);
        return (*_choices[distribution(_rng)]);
    }

    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        choose().append(key, builder);
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        choose().append(builder);
    }

protected:
    DefaultRandom& _rng;
    ActorId _id;
    std::vector<UniqueAppendable> _choices;
    std::vector<int64_t> _weights;
    int32_t _elemNumber;
    bool _deterministic;
};

// This is a a more specific version of ChooseGenerator that produces strings. It is only used
// within the JoinGenerator.
class ChooseStringGenerator : public Generator<std::string> {
public:
    ChooseStringGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng}, _id{generatorArgs.actorId} {
        if (node["deterministic"] && node["weights"]) {
            std::stringstream msg;
            msg << "Invalid Syntax for choose: cannot have both 'deterministic' and 'weights'";
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
        if (!node["from"].isSequence()) {
            std::stringstream msg;
            msg << "Malformed node for choose from array. Not a sequence " << node;
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
        for (const auto&& [k, v] : node["from"]) {
            _choices.push_back(stringGenerator(v, generatorArgs));
        }
        if (node["weights"]) {
            for (const auto&& [k, v] : node["weights"]) {
                _weights.push_back(v.to<int64_t>());
            }
        } else {
            // If not passed in, give each choice equal weight
            _weights.assign(_choices.size(), 1);
        }
        if (node["deterministic"]) {
            _deterministic = node["deterministic"].maybe<bool>().value_or(false);
        } else {
            _deterministic = false;
        }
        _elemNumber = -1;
    }

    std::string evaluate() override {
        if (_deterministic) {
            ++_elemNumber;
            return (_choices[_elemNumber % _choices.size()]->evaluate());
        }
        // Pick a random number between 0 and sum(weights)
        // Pick value based on that.
        auto distribution = boost::random::discrete_distribution(_weights);
        return (_choices[distribution(_rng)]->evaluate());
    };

protected:
    DefaultRandom& _rng;
    ActorId _id;
    std::vector<UniqueGenerator<std::string>> _choices;
    std::vector<int64_t> _weights;
    int32_t _elemNumber;
    bool _deterministic;
};

/**
 * FrequencyMaps
 * A list of {string, count} pairs.
 * An item will be pulled from a random bucket and its count will be decremented. If all buckets are
 * empty, the generator throws an error.
 */
class FrequencyMapGenerator : public Generator<std::string> {
public:
    FrequencyMapGenerator(const Node& node, GeneratorArgs generatorArgs) : _rng{generatorArgs.rng} {
        if (!node["from"].isMap()) {
            std::stringstream msg;
            msg << "Malformed node for 'TakeRandomStringFromFrequencyMap' from a map " << node;
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }

        for (const auto&& [k, v] : node["from"]) {
            _map.push_back(v.key(), v.to<std::size_t>());
        }
    }

    std::string evaluateWithRng(DefaultRandom& rng) {
        if (_map.size() == 1) {
            return _map.take(0);
        }

        // Pick a random number between 0 and _map.size() inclusive
        auto distribution = boost::random::uniform_int_distribution<size_t>(0, _map.size() - 1);
        auto value = distribution(rng);
        return _map.take(value);
    }

    std::string evaluate() override {
        return evaluateWithRng(_rng);
    };

private:
    DefaultRandom& _rng;
    v1::FrequencyMap _map;
};

/**
 * Singleton FrequencyMaps
 * Keep singletons shared frequency maps shared across threads.
 * This can be used to ensure the distribution across threads matches exactly. This is important if
 * you want a single value for given thread, but use multiple threads to load the data. Maps are
 * identified by their "id" are shared.
 */
class FrequencyMapSingletonGenerator : public Generator<std::string> {
public:
    FrequencyMapSingletonGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng} {
        _id = node["id"].maybe<std::string>().value();

        // Keep a singleton of these generators by id
        {
            std::lock_guard<std::mutex> lck(_mutex);

            if (_generators.count(_id) == 0) {
                FrequencyMapGenerator gen(node, generatorArgs);
                _generators.insert({_id, gen});
            }
        }
    }

    std::string evaluate() override {
        std::lock_guard<std::mutex> lck(_mutex);
        return _generators.at(_id).evaluateWithRng(_rng);
    };

private:
    std::string _id;
    DefaultRandom& _rng;

    static std::unordered_map<std::string, FrequencyMapGenerator> _generators;
    static std::mutex _mutex;
};

std::unordered_map<std::string, FrequencyMapGenerator> FrequencyMapSingletonGenerator::_generators;
std::mutex FrequencyMapSingletonGenerator::_mutex;

class IPGenerator : public Generator<std::string> {
public:
    IPGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _subnetMask{std::numeric_limits<uint32_t>::max()},
          _prefix{} {}

    std::string evaluate() override {
        // Pick a random 32 bit integer
        // Bitwise add with _subnetMask and add to _prefix
        // Note that _subnetMask and _prefix are always default values for now.
        auto distribution = boost::random::uniform_int_distribution<int32_t>{};
        auto ipint = (distribution(_rng) & _subnetMask) + _prefix;
        int32_t octets[4];
        for (int i = 0; i < 4; i++) {
            octets[i] = ipint & 255;
            ipint = ipint >> 8;
        }
        // convert to a string
        std::ostringstream ipout;
        ipout << octets[3] << "." << octets[2] << "." << octets[1] << "." << octets[0];
        return ipout.str();
    }

protected:
    DefaultRandom& _rng;
    ActorId _id;
    uint32_t _subnetMask;
    uint32_t _prefix;
};


// Currently can only join stringGenerators. Ideally any generator would be able to be
// transformed into a string. To work around this there is a ChooseStringGenerator in addition to
// the ChooseGenerator, to allow embedding a choose node within a join node, but ideally it would
// not be neded.
class JoinGenerator : public Generator<std::string> {
public:
    JoinGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _separator{node["sep"].maybe<std::string>().value_or("")} {
        if (!node["array"].isSequence()) {
            std::stringstream msg;
            msg << "Malformed node for join array. Not a sequence " << node;
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
        for (const auto&& [k, v] : node["array"]) {
            _parts.push_back(stringGenerator(v, generatorArgs));
        }
    }
    std::string evaluate() override {
        std::ostringstream output;
        bool first = true;
        for (auto&& part : _parts) {
            if (first) {
                first = false;
            } else {
                output << _separator;
            }
            output << part->evaluate();
        }
        return output.str();
    }

protected:
    DefaultRandom& _rng;
    ActorId _id;
    std::vector<UniqueGenerator<std::string>> _parts;
    std::string _separator;
};

/** handle the formatting of an individual array element value with the latest format. */
void format_element(boost::format& message, bsoncxx::document::element val) {
    switch (val.type()) {
        case bsoncxx::v_noabi::type::k_utf8:
            message % val.get_string().value.to_string();
            return;
        case bsoncxx::type::k_int32:
            message % val.get_int32().value;
            return;
        case bsoncxx::type::k_int64:
            message % val.get_int64().value;
            return;
        case bsoncxx::type::k_document:
            message % bsoncxx::to_json(val.get_document().value);
            return;
        case bsoncxx::type::k_array:
            message % bsoncxx::to_json(val.get_array().value);
            return;
        case bsoncxx::type::k_oid:
            message % val.get_oid().value.to_string();
            return;
        case bsoncxx::type::k_binary:
            message % val.get_binary().bytes;
            return;
        case bsoncxx::type::k_bool:
            message % val.get_bool();
            return;
        case bsoncxx::type::k_code:
            message % val.get_code().code;
            return;
        case bsoncxx::type::k_codewscope:
            message % val.get_codewscope().code;
            return;
        case bsoncxx::type::k_date:
            message % val.get_date();
            return;
        case bsoncxx::type::k_double:
            message % val.get_double();
            return;
        case bsoncxx::type::k_null:
            message % "null";
            return;
        case bsoncxx::type::k_undefined:
            message % "undefined";
            return;
        case bsoncxx::type::k_timestamp:
            // Ignoring increment.
            message % val.get_timestamp().timestamp;
            return;
        case bsoncxx::type::k_regex:
            // Ignoring options.
            message % val.get_regex().regex;
            return;
        case bsoncxx::type::k_minkey:
            message % "minkey";
            return;
        case bsoncxx::type::k_maxkey:
            message % "maxkey";
            return;
        case bsoncxx::type::k_decimal128:
            message % val.get_decimal128().value.to_string();
            return;
        case bsoncxx::type::k_symbol:
            message % val.get_symbol().symbol;
            return;
        case bsoncxx::type::k_dbpointer:
            message % val.get_dbpointer().value.to_string();
            return;
        default:
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("format_element: unreachable code"));
    }
}

/** `{^FormatString: {"format": "% 4s%04d%s", "withArgs": ["c", 3.1415, {^RandomInt: {min: 0, max:
 * 999}}, {^RandomString: {length: 20, alphabet: b}}]}}` */
class FormatStringGenerator : public Generator<std::string> {
public:
    FormatStringGenerator(const Node& node,
                          GeneratorArgs generatorArgs,
                          std::map<std::string, Parser<UniqueAppendable>> parsers)
        : _rng{generatorArgs.rng}, _format{node["format"].maybe<std::string>().value_or("")} {
        std::stringstream msg;
        if (!node["format"]) {
            msg << "Malformed FormatString: format missing '" << _format << "'" << node << "\n";
        } else if (_format.empty()) {
            msg << "Malformed FormatString: format cannot be empty '" << _format << "'" << node
                << "\n";
        }

        if (!node["withArgs"]) {
            msg << "Malformed FormatString: withArgs missing." << node;
        } else if (!node["withArgs"].isSequence()) {
            msg << "Malformed FormatString:  withArgs " << node.type() << " not a sequence "
                << node;
        }

        if (!msg.str().empty()) {
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }

        for (const auto&& [k, v] : node["withArgs"]) {
            _arguments.push_back(
                valueGenerator<false, UniqueAppendable>(v, generatorArgs, parsers));
        }
    }
    std::string evaluate() override {
        boost::format message(_format);
        const std::string key{"current"};
        bsoncxx::builder::basic::document argumentBuilder{};

        for (auto&& argumentGen : _arguments) {
            argumentGen->append(key, argumentBuilder);

            format_element(message, argumentBuilder.view()[key]);
            argumentBuilder.clear();
        }
        return message.str();
    }

protected:
    DefaultRandom& _rng;
    std::string _format;
    std::vector<UniqueAppendable> _arguments;
};

/** `{^ObjectId: {hex: "61158c40f2a806ab5ed16548"}}` */
class ObjectIdGenerator : public Generator<bsoncxx::types::b_oid> {
public:
    ObjectIdGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng}, _node{node}, _hexGen{stringGenerator(node, generatorArgs)} {}

    bsoncxx::types::b_oid evaluate() override {
        auto hex = _hexGen->evaluate();
        auto objectId = bsoncxx::types::b_oid{hex.empty() ? oid{} : oid{hex}};
        return objectId;
    }

private:
    DefaultRandom& _rng;
    const Node& _node;
    const UniqueGenerator<std::string> _hexGen;
};


std::string validUuidChars{"0123456789abcdefABCDEF-"};
std::string validateUuidPattern{"^([0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12})$"};
std::regex validateUuidRegex{validateUuidPattern, std::regex_constants::icase};

/**
 * Validate the uuid hex format.
 * @param hex
 *   the hex string
 * @raises InvalidValueGeneratorSyntax if the format is not correct
 *  The correct format is an 8-4-4-4-12 hex string (case is ignored)
 */
void validateUuidHex(std::string hex) {

    auto pos = hex.find_first_not_of(validUuidChars);
    if ( pos != std::string::npos ) {
        std::stringstream msg;
        std::string invalid(hex);
        invalid.erase(std::remove_if(invalid.begin(), invalid.end(),
                                 [&](char c) { return validUuidChars.find(c) != std::string::npos; } ),
                      invalid.end());

        msg << "'" << hex << "' contains invalid characters '" << invalid << "'";
        BOOST_LOG_TRIVIAL(warning) << " UuidGenerator " << msg.str();
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
    }

    std::smatch match;
    if (!std::regex_search(hex, match, validateUuidRegex)) {
        std::stringstream msg;
        msg << "'" << hex << "' is not a valid format. The format must match: '" << validateUuidPattern << "'";
        BOOST_LOG_TRIVIAL(warning) << " UuidGenerator " << msg.str();
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
    }
}

/**
 * Validate that the uuid sub type is correct.
 * @param sub_type
 *   the binary sub type value
 * @raises InvalidValueGeneratorSyntax if the sub type is not correct
*  Currntly only supports sub type 4.
 */
void validateUuidSubType(bsoncxx::binary_sub_type sub_type) {

    if (bsoncxx::binary_sub_type::k_uuid != sub_type) {
        std::stringstream msg;
        msg << "Invalid binary sub_type. UUID only supports bsoncxx::binary_sub_type::k_uuid("
            << static_cast<int64_t>(bsoncxx::binary_sub_type::k_uuid) << "), got " << static_cast<int64_t>(sub_type);
        BOOST_LOG_TRIVIAL(warning) << " UuidGenerator " << msg.str();
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
    }
}

/**
 * Convert ascii hex nibble to binary.
 * @param c
 *   the ascii hex nibble
 * @return
 *   the binary version of the hex nibble
 */
uint8_t hexNibbleToBin(uint8_t c) {
    if (c >= '0' && c <= '9') {
        c -= '0';
    } else  if (c >= 'a' && c <= 'f') {
        c = c - 'a' + 10;
    } else  if (c >= 'A' && c <= 'F') {
        c = c - 'A' + 10;
    }
    return c;
}

/**
 * Convert hex string Uuid to binary.
 * @param hex
 *   the input hex string, '-' will be ignored.
 * @param uuid
 *   the array to store the uuid.
 * @return the output array
 */
uint8_t* hex2BinUuid(std::string hex, uint8_t* uuid) {
    hex.erase(std::remove_if(hex.begin(), hex.end(),
                             [&](char c) { return c == '-'; } ),
              hex.end());

    auto i = 0;
    for(std::string::iterator it = hex.begin(); it != hex.end(); ++it) {
        uuid[i++] = (hexNibbleToBin(*it) << 4) + hexNibbleToBin(*++it);
    }
    return uuid;
}

/**
 * `{^UUID: {hex: "3b241101-e2bb-4255-8caf-4136c566a962"} }`
 *  Generator for non-legacy UUIDs (0x04). hex can itself be a generator so the string can be
 *  generated in many fashions ^Join, ^FormatString, ... etc.
 *
 *  The input format accepted for the hex field is 8-4-4-4-12 format. The code is agnostic as to the
 *  UUID version.
 *
 *  see https://en.wikipedia.org/wiki/Universally_unique_identifier
 */
class UuidGenerator : public Generator<bsoncxx::types::b_binary> {
public:
    UuidGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _node{node},
          _hexGen{stringGenerator(node["hex"], generatorArgs)},
          // If there is no subType then use bsoncxx::binary_sub_type::k_uuid
          _subTypeGen{node["subType"]
                          ? intGenerator(node["subType"], generatorArgs)
                          : std::make_unique<ConstantAppender<int64_t>>(
                                static_cast<int64_t>(bsoncxx::binary_sub_type::k_uuid))} {}

    bsoncxx::types::b_binary evaluate() override {
        auto hex = _hexGen->evaluate();
        validateUuidHex(hex);

        auto sub_type = static_cast<bsoncxx::binary_sub_type>(_subTypeGen->evaluate());
        validateUuidSubType(sub_type);

        return bsoncxx::types::b_binary{sub_type, 16, hex2BinUuid(hex, uuid)};
    }

private:
    DefaultRandom& _rng;
    const Node& _node;
    const UniqueGenerator<std::string> _hexGen;
    const UniqueGenerator<int64_t> _subTypeGen;
    uint8_t uuid[16];

};

class StringGenerator : public Generator<std::string> {
public:
    StringGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _id{generatorArgs.actorId},
          _lengthGen{intGenerator(extract(node, "length", "^RandomString"), generatorArgs)},
          _alphabet{node["alphabet"].maybe<std::string>().value_or(kDefaultAlphabet)},
          _alphabetLength{_alphabet.size()} {
        if (_alphabetLength <= 0) {
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(
                "Random string requires non-empty alphabet if specified"));
        }
    }

protected:
    DefaultRandom& _rng;
    ActorId _id;
    UniqueGenerator<int64_t> _lengthGen;
    std::string _alphabet;
    const size_t _alphabetLength;
};


class DataSetCache {
public:
    DataSetCache() {}

    using LoadFunction = std::function<void(std::vector<std::string>*)>;
    /* The dataset is stored in a map, using the path of the file as the key and a Dataset object,
    which contains the actual data as well as whether it is done being loaded,as the value.
    In this method, the dataset at the given path is filled by the load function, and a reference
    to the data is returned. getDataSetForPath method doesn't need the lock because loading and
    evaluating are separate stages in Genny */
    std::vector<std::string>& loadDataset(const std::string& path, LoadFunction loader) {
        {
            std::unique_lock<std::mutex> lk(_dataset_mutex);
            /* If the path already exists, it is either loaded or being loaded, so this thread
            should not do any loading. However, it should wait until loading is finished. */
            if (_all_datasets.count(path) > 0) {
                /* When multiple threads try to load a dataset, all but the first thread will just
                wait for the loading to finish. This way only the first thread will actually load
                the data. Once the first vector is inserted and the first thread has finished
                loading, it will signal all waiting threads, and those that were waiting on the
                same path as was loaded will finish waiting and return the data. */
                _dataset_done_cv.wait(lk, [&]() {return _all_datasets[path].done; });
                return _all_datasets[path].data;
            }

            /* Insert an empty vector to denote that we have won the race */
            _all_datasets[path] = {false, std::vector<std::string>()};
        }
        // Only the first thread should ever get here. It should load the data, set the done flag to
        // true, and signal cond waiters that a dataset has been loaded.
        loader(&_all_datasets[path].data);
        {
            // We have to lock while setting done to avoid a waiting thread missing the done flag.
            std::unique_lock<std::mutex> lk(_dataset_mutex);
            _all_datasets[path].done = true;
        }
        _dataset_done_cv.notify_all();
        return _all_datasets[path].data;
    }

    const std::vector<std::string>& getDatasetForPath(const std::string& path) {
        return _all_datasets[path].data;
    }

private:
    struct Dataset {
        bool done;
        std::vector<std::string> data;
    };
    std::unordered_map<std::string, Dataset> _all_datasets;
    std::mutex _dataset_mutex;
    std::condition_variable _dataset_done_cv;
};

// The ChooseStringFromDataset* generators will use the same static dataset stored in this class.
class WithDatasetStorage {
protected:
    WithDatasetStorage(const std::string& path) : _path(path) {
        if (_path.empty()) {
            BOOST_THROW_EXCEPTION(
                InvalidValueGeneratorSyntax("Dataset requires non-empty path"));
        }
        loadDataset();
        // We can store a pointer to this data because the dataset never moves once created.
        _dataset = &_datasets.getDatasetForPath(_path);
    }

    const std::vector<std::string>& getDataset() {
        return *_dataset;
    }

private:
    void loadDataset() {
        auto& dataset = _datasets.loadDataset(_path, [this](std::vector<std::string>* to) { readFile(to); });

        if (dataset.empty()) {
            boost::filesystem::path cwd(boost::filesystem::current_path());

            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(
                "The specified file for ChooseFromDataset is empty. Specified path: " + _path +
                ". Current Working Directory: " + cwd.string()));
        }
    }

    /* Read the file, line by line, and store it in the vector */
    void readFile(std::vector<std::string>* result) {
        std::ifstream ifs;
        std::string currentLine;
        ifs.open(_path, std::ifstream::in);
        if (!ifs.is_open()) {
            boost::filesystem::path cwd(boost::filesystem::current_path());
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(
                "The specified file for ChooseFromDataset cannot be opened or it does "
                "not exist. Specified path: " +
                _path + ". Current Working Directory: " + cwd.string()));
        }

        while (std::getline(ifs, currentLine)) {
            /* Ignore emtpy lines */
            if (!currentLine.empty()) {
                result->push_back(currentLine);
            }
        }
    }
private:
    std::string _path;
    static inline DataSetCache _datasets;
    const std::vector<std::string>* _dataset;
};

class ChooseStringFromDatasetRandomly : public Generator<std::string>, private WithDatasetStorage {
public:
    ChooseStringFromDatasetRandomly(const Node& node, GeneratorArgs generatorArgs)
        : WithDatasetStorage(node["path"].maybe<std::string>().value()),
          _rng{generatorArgs.rng} {}

    std::string evaluate() {
        const auto& dataset = getDataset();
        auto distribution = boost::random::uniform_int_distribution<size_t>{0, dataset.size() - 1};
        return dataset[distribution(_rng)];
    }

private:
    DefaultRandom& _rng;
};

class ChooseStringFromDatasetSequentially : public Generator<std::string>, private WithDatasetStorage {
public:
    ChooseStringFromDatasetSequentially(const Node& node, GeneratorArgs generatorArgs)
        : WithDatasetStorage(node["path"].maybe<std::string>().value()),
          _line(node["startFromLine"].maybe<uint64_t>().value_or(0)) {
        if(_line >= getDataset().size()) {
            BOOST_THROW_EXCEPTION(
                InvalidValueGeneratorSyntax("In ChooseFromDataset, startFromLine was out of range of the provided file"));
        }
    }

    std::string evaluate() {
        const auto& dataset = getDataset();
        auto next = dataset[_line];
        _line = (_line + 1) % dataset.size();
        return next;
    }

private:
    uint64_t _line;
};

/** `{^RandomString:{...}` */
class NormalRandomStringGenerator : public StringGenerator {
public:
    /**
     * @param node `{length:<int>, alphabet:opt string}`
     */
    NormalRandomStringGenerator(const Node& node, GeneratorArgs generatorArgs)
        : StringGenerator(node, generatorArgs) {}

    std::string evaluate() override {
        auto distribution = boost::random::uniform_int_distribution<size_t>{0, _alphabetLength - 1};

        auto length = _lengthGen->evaluate();
        std::string str(length, '\0');

        for (int i = 0; i < length; ++i) {
            str[i] = _alphabet[distribution(_rng)];
        }

        return str;
    }
};

/** `{^FastRandomString:{...}` */
class FastRandomStringGenerator : public StringGenerator {
public:
    /** @param node `{length:<int>, alphabet:opt str}` */
    FastRandomStringGenerator(const Node& node, GeneratorArgs generatorArgs)
        : StringGenerator(node, generatorArgs) {}

    std::string evaluate() override {
        auto length = _lengthGen->evaluate();
        std::string str(length, '\0');

        auto randomValue = _rng();
        int bits = 64;

        for (int i = 0; i < length; ++i) {
            if (bits < 6) {
                randomValue = _rng();
                bits = 64;
            }

            str[i] = _alphabet[(randomValue & 0x2f) % _alphabetLength];
            randomValue >>= 6;
            bits -= 6;
        }
        return str;
    }
};

// The following is a replacement for partial template specialization for functions, which is not
// currently supported in C++.
template <typename F, typename T>
struct convert {};

// Conversion used between int, long, double
template <typename F, typename T>
T convertToNumeric(convert<F, T>, const F& from) {
    return (T)from;
}

// Special conversion for decimal
template <typename F>
bsoncxx::decimal128 convertToNumeric(convert<F, bsoncxx::decimal128>, const F& from) {
    // mongocxx doesn't define nice converters from numeric types to decimal, so we convert first
    // to string and then to decimal
    return bsoncxx::decimal128(std::to_string(from));
}

// String conversions
bsoncxx::decimal128 convertToNumeric(convert<std::string, bsoncxx::decimal128>,
                                     const std::string& s) {
    return bsoncxx::decimal128(s);
}
double convertToNumeric(convert<std::string, double>, const std::string& s) {
    return stod(s);
}
int64_t convertToNumeric(convert<std::string, int64_t>, const std::string& s) {
    return stol(s);
}
int32_t convertToNumeric(convert<std::string, int32_t>, const std::string& s) {
    return stoi(s);
}

// This is the only overload we run directly, and it will route to the correct template.
// By constructing and passing a convert struct with the <F, T> type, we will route to the desired
// overload of convertToNumeric matching that type.
template <typename F, typename T>
T convertToNumeric(const F& from) {
    return convertToNumeric(convert<F, T>{}, from);
}

// overloaded struct for use with std::variants.
// Reference: https://en.cppreference.com/w/cpp/utility/variant/visit
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Generator to convert from any numeric type except decimal or string to specific numeric type.
template <typename T>
class NumericConversionGenerator : public Generator<T> {
private:
    static inline void fail_multiple_convert() {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(
            "When determining type of the from generator, multiple conversions succeeded -- "
            "generator type is ambiguous. This should not happen."));
    }

public:
    NumericConversionGenerator(const Node& node, GeneratorArgs generatorArgs) {
        // First, if "from" is a constant, we can always parse it as a constant string generator,
        // even if it also parses as a constant numeric generator.
        try {
            _generator =
                std::make_unique<ConstantAppender<std::string>>(node["from"].to<std::string>());
            return;
        } catch (const InvalidConversionException& e) {
        } catch (const UnknownParserException& e) {
        }

        // Otherwise, "from" should be a generator -- try all of the possible generators for from,
        // and ensure that exactly one works.
        bool succeeded = false;
        try {
            _generator = stringGenerator(node["from"], generatorArgs);
            succeeded = true;
        } catch (const InvalidConversionException& e) {
        } catch (const UnknownParserException& e) {
        }
        try {
            auto generator = intGenerator(node["from"], generatorArgs);
            if (succeeded) {
                // generator conversion has succeeded more than once, so node["from"] is ambiguous.
                // This is bad; panic.
                fail_multiple_convert();
            }
            _generator = std::move(generator);
            succeeded = true;
        } catch (const InvalidConversionException& e) {
        } catch (const UnknownParserException& e) {
        }
        try {
            auto generator = doubleGenerator(node["from"], generatorArgs);
            if (succeeded) {
                fail_multiple_convert();
            }
            _generator = std::move(generator);
            succeeded = true;
        } catch (const InvalidConversionException& e) {
        } catch (const UnknownParserException& e) {
        }
        try {
            auto generator = int32Generator(node["from"], generatorArgs);
            if (succeeded) {
                fail_multiple_convert();
            }
            _generator = std::move(generator);
            succeeded = true;
        } catch (const InvalidConversionException& e) {
        } catch (const UnknownParserException& e) {
        }
        if (!succeeded) {
            // No generator conversion was successful, this is a failure.
            BOOST_THROW_EXCEPTION(
                InvalidValueGeneratorSyntax("Numeric conversion generator only supports "
                                            "conversions from the string, double, int, "
                                            "and int32 types. The type of the \"from\" field did "
                                            "not match any of those types."));
        }
    }
    T evaluate() override {
        return std::visit(overloaded{[](UniqueGenerator<std::string>& gen) {
                                         return convertToNumeric<std::string, T>(gen->evaluate());
                                     },
                                     [](UniqueGenerator<int64_t>& gen) {
                                         return convertToNumeric<int64_t, T>(gen->evaluate());
                                     },
                                     [](UniqueGenerator<double>& gen) {
                                         return convertToNumeric<double, T>(gen->evaluate());
                                     },
                                     [](UniqueGenerator<int32_t>& gen) {
                                         return convertToNumeric<int32_t, T>(gen->evaluate());
                                     }},
                          _generator);
    }

private:
    std::variant<UniqueGenerator<std::string>,
                 UniqueGenerator<int64_t>,
                 UniqueGenerator<double>,
                 UniqueGenerator<int32_t>>
        _generator;
};

/** `{^ActorId: {}}` */
class ActorIdIntGenerator : public Generator<int64_t> {
public:
    ActorIdIntGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _actorId{generatorArgs.actorId} {}
    int64_t evaluate() override {
        return _actorId;
    }

private:
    int64_t _actorId;
};

/** `{^Null: {}}` */
class NullGenerator : public Generator<bsoncxx::types::b_null> {
public:
    NullGenerator(const Node& node, GeneratorArgs generatorArgs) {}
    bsoncxx::types::b_null evaluate() override {
        return bsoncxx::types::b_null{};
    }
};

/** `{^ActorIdString: {}}` */
class ActorIdStringGenerator : public Generator<std::string> {
public:
    ActorIdStringGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _actorId{std::to_string(generatorArgs.actorId)} {}
    std::string evaluate() override {
        return _actorId;
    }

private:
    std::string _actorId;
};

/** `{^NowTimestamp: {}}` */
// For Timesamp, the default start value for .increment is 1 and .timestamp is current time since
// UNIX epoch in seconds. 
// see https://www.mongodb.com/docs/mongodb-shell/reference/data-types/#timestamp for more details.
class NowTimestampGenerator : public Generator<bsoncxx::types::b_timestamp> {
public:
    NowTimestampGenerator(const Node& node, GeneratorArgs generatorArgs) {}
    bsoncxx::types::b_timestamp evaluate() override {
        auto now = std::chrono::system_clock::now();
        uint32_t nowSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return bsoncxx::types::b_timestamp{.increment = 1, .timestamp = nowSinceEpoch};
    }
};

/** `{^Now: {}}` */
class NowGenerator : public Generator<bsoncxx::types::b_date> {
public:
    NowGenerator(const Node& node, GeneratorArgs generatorArgs) {}
    bsoncxx::types::b_date evaluate() override {
        return bsoncxx::types::b_date{std::chrono::system_clock::now()};
    }
};

// The following formats also cover "%Y-%m-%d". Timezones require local_time_input_facet AND
// local_date_time see https://www.boost.org/doc/libs/1_75_0/doc/html/date_time/date_time_io.html.
// We strive to use smart pointers where possible. In this case this is not possible
// but not a huge deal as these objects are statically allocated.
const static auto formats = {
    std::locale(std::locale::classic(),
                new boost::local_time::local_time_input_facet("%Y-%m-%dT%H:%M:%s%ZP")),
    std::locale(std::locale::classic(),
                new boost::local_time::local_time_input_facet("%Y-%m-%d %H:%M:%s%ZP")),
    std::locale(std::locale::classic(),
                new boost::local_time::local_time_input_facet("%Y-%m-%dT%H:%M:%S%F%ZP *")),
};

class DateToIntGenerator : public Generator<int64_t> {
public:
    explicit DateToIntGenerator(UniqueGenerator<bsoncxx::types::b_date>&& generator)
        : _generator{std::move(generator)} {}
    int64_t evaluate() override {
        auto date = _generator->evaluate();
        return date.to_int64();
    }

private:
    const UniqueGenerator<bsoncxx::types::b_date> _generator;
};

class CastDoubleToInt64Generator : public Generator<int64_t> {
public:
    explicit CastDoubleToInt64Generator(UniqueGenerator<double>&& generator)
        : _generator{std::move(generator)} {}
    int64_t evaluate() override {
        return (int64_t)_generator->evaluate();
    }

private:
    const UniqueGenerator<double> _generator;
};

class DateStringParserGenerator : public Generator<int64_t> {
public:
    explicit DateStringParserGenerator(UniqueGenerator<std::string>&& generator)
        : _generator{std::move(generator)} {}
    int64_t evaluate() override {
        auto datetime = _generator->evaluate();
        return parseStringToMillis(datetime);
    }

private:
    const UniqueGenerator<std::string> _generator;
};

/** `{^Date: "2015-01-01"}` */
class DateGenerator : public Generator<bsoncxx::types::b_date> {
public:
    DateGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _date{dateGenerator(node, generatorArgs)} {}

    bsoncxx::types::b_date evaluate() override {
        auto millis = _date->evaluate();
        return bsoncxx::types::b_date{std::chrono::milliseconds{millis}};
    }

private:
    const UniqueGenerator<int64_t> _date;
};

/** `{^BinData: {numBytes: 32}}` */
class BinDataGenerator : public Generator<bsoncxx::types::b_binary> {
private:
    using bintype = bsoncxx::binary_sub_type;

public:
    BinDataGenerator(const Node& node,
                     GeneratorArgs generatorArgs,
                     const bintype binDataType = bintype::k_binary)
        : _node{node}, _binData(genRandBinData(node, binDataType)) {}

    bsoncxx::types::b_binary evaluate() override {
        return _binData;
    }

    bsoncxx::types::b_binary genRandBinData(const Node& node, const bintype binDataType) {
        int64_t numBytes = node["numBytes"].maybe<int64_t>().value_or(32);
        uint8_t bytesArr[numBytes];
        for (int i = 0; i < numBytes; i++) {
            bytesArr[i] = rand();
        }
        return bsoncxx::types::b_binary{
            binDataType, static_cast<uint32_t>(sizeof(bytesArr)), bytesArr};
    }

private:
    bsoncxx::types::b_binary _binData;
    const Node& _node;
};

/** `{^IncDate: {start: "2022-01-01", step: 10000, multiplier: 0}}` */
class IncDateGenerator : public Generator<bsoncxx::types::b_date> {
public:
    IncDateGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _step{node["step"].maybe<int64_t>().value_or(1)} {
        _counter = dateGenerator(node["start"], generatorArgs)->evaluate() +
            generatorArgs.actorId * node["multiplier"].maybe<int64_t>().value_or(0);
    }

    bsoncxx::types::b_date evaluate() override {
        auto inc_value = _counter;
        _counter += _step;
        return bsoncxx::types::b_date{std::chrono::milliseconds{inc_value}};
    }

private:
    int64_t _step;
    int64_t _counter;
};

/** `{^RandomDate: {min: "2015-01-01", max: "2015-01-01T23:59:59.999Z"}}` */
class RandomDateGenerator : public Generator<bsoncxx::types::b_date> {
public:
    RandomDateGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng},
          _node{node},
          _minGen{dateGenerator(node["min"], generatorArgs)},
          _maxGen{dateGenerator(node["max"], generatorArgs, max_date)} {}

    bsoncxx::types::b_date evaluate() override {
        auto min = _minGen->evaluate();
        auto max = _maxGen->evaluate();
        if (max <= min) {
            std::ostringstream msg;
            msg << "^RandomDate: " << _node << ", max (" << max << ") must be greater than min ("
                << min << ")";
            BOOST_LOG_TRIVIAL(warning) << " RandomDateGenerator " << msg.str();
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax{msg.str()});
        }
        boost::random::uniform_int_distribution<long long> dist{min, max - 1};
        return bsoncxx::types::b_date{std::chrono::milliseconds{dist(_rng)}};
    }

private:
    DefaultRandom& _rng;
    const Node& _node;
    const UniqueGenerator<int64_t> _minGen;
    const UniqueGenerator<int64_t> _maxGen;
};

class CycleGenerator : public Appendable {

public:
    CycleGenerator(const Node& node,
                   GeneratorArgs generatorArgs,
                   std::map<std::string, Parser<UniqueAppendable>> parsers)
        : CycleGenerator(
              node, generatorArgs, parsers, extract(node, "ofLength", "^Cycle").to<int64_t>()) {}

    CycleGenerator(const Node& node,
                   GeneratorArgs generatorArgs,
                   std::map<std::string, Parser<UniqueAppendable>> parsers,
                   int64_t ofLength)
        : _ofLength{ofLength},
          _cache{generateCache(
              extract(node, "fromGenerator", "^Cycle"), generatorArgs, parsers, _ofLength)},
          _currentIndex{0} {}

    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        auto cacheView = _cache.view();
        builder.append(bsoncxx::builder::basic::kvp(key, cacheView[_currentIndex].get_value()));
        updateIndex();
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        auto cacheView = _cache.view();
        builder.append(cacheView[_currentIndex].get_value());
        updateIndex();
    }

private:
    static bsoncxx::array::value generateCache(
        const Node& node,
        GeneratorArgs generatorArgs,
        std::map<std::string, Parser<UniqueAppendable>> parsers,
        int64_t size) {
        bsoncxx::builder::basic::array builder{};
        auto valueGen = valueGenerator<false, UniqueAppendable>(node, generatorArgs, parsers);

        for (int64_t i = 0; i < size; i++) {
            valueGen->append(builder);
        }
        return builder.extract();
    }

    void updateIndex() {
        _currentIndex = (_currentIndex + 1) % _ofLength;
    }

    int64_t _ofLength;
    bsoncxx::array::value _cache;
    int64_t _currentIndex;
};

/** `{^Repeat: {numRepeats: 2, fromGenerator: {^Inc: {start: 1000}}}}` */
class RepeatGenerator : public Appendable {

public:
    RepeatGenerator(const Node& node,
                    GeneratorArgs generatorArgs,
                    std::map<std::string, Parser<UniqueAppendable>> parsers)
        : RepeatGenerator(
              node, generatorArgs, parsers, extract(node, "count", "^Repeat").to<int64_t>()) {}

    RepeatGenerator(const Node& node,
                    GeneratorArgs generatorArgs,
                    std::map<std::string, Parser<UniqueAppendable>> parsers,
                    int64_t numRepeats)
        : _numRepeats{numRepeats},
          _repeatCounter{0},
          _valueGen{valueGenerator<false, UniqueAppendable>(
              extract(node, "fromGenerator", "^Repeat"), generatorArgs, parsers)},
          _item(getItem()) {}

    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        auto itemView = _item.view();
        builder.append(bsoncxx::builder::basic::kvp(key, itemView[0].get_value()));
        updateIndex();
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        auto itemView = _item.view();
        builder.append(itemView[0].get_value());
        updateIndex();
    }

private:
    bsoncxx::array::value getItem() {
        bsoncxx::builder::basic::array builder{};
        _valueGen->append(builder);
        return builder.extract();
    }

    void updateIndex() {
        _repeatCounter++;
        if (_repeatCounter >= _numRepeats) {
            _repeatCounter = 0;
            _item = getItem();
        }
    }

    int64_t _numRepeats;
    int64_t _repeatCounter;
    UniqueAppendable _valueGen;
    bsoncxx::array::value _item;
};

/** `{^Array: {of: {a: b}, number: 2}` */
class ArrayGenerator : public Generator<bsoncxx::array::value> {
public:
    ArrayGenerator(const Node& node,
                   GeneratorArgs generatorArgs,
                   std::map<std::string, Parser<UniqueAppendable>> parsers)
        : _rng{generatorArgs.rng},
          _node{node},
          _generatorArgs{generatorArgs},
          _valueGen{valueGenerator<false, UniqueAppendable>(node["of"], generatorArgs, parsers)},
          _nItemsGen{intGenerator(extract(node, "number", "^Array"), generatorArgs)},
          _distinct{node["distinct"] ? node["distinct"].maybe<bool>().value_or(false) : false} {}

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder{};
        const auto nItems = _nItemsGen->evaluate();
        /* If _distinct is true, then the generated array must contain unique items.
         *
         * Uniqueness for individual elements is based on the hash value (as defined in
         * Appendable::SetHasher) and the == operator (as defined in Appendable::SetKeyEq).
         *
         * An unordered set is used to check for uniqueness. To simplify the implementation, we wrap
         * all generated values in a `bsoncxx::array::value` to store in the set by appending to a
         * temporary array builder (`tempBuilder`) and extracting the singular array element.
         *
         * A failed insertion into the set indicates that the value is duplicate, and we ignore it
         * and do not add it to the builder. We keep pulling values from _valueGen until either we
         * have `number` unique elements or pulled duplicate values `consecutiveRepeatsThreshold`
         * times in a row. In the latter case, we assume that we are unlikely to find `number`
         * unique elements in a reasonable amount of time, if at all, so we throw an exception.
         *
         * If a value is unique, we insert it into `builder`. Since the underlying BSON value can be
         * any arbitrary BSON value type, we use a strategy employed by the BSON CXX driver library
         * to extract the underlying BSON value with a switch-case statement and append it to
         * `builder`.
         */
        if (_distinct) {
            // Choose initial bucket size of 2 * nItems to minimize hash collisions and resizes.
            std::unordered_set<bsoncxx::array::value, SetHasher, SetKeyEq> distinctValues(2 *
                                                                                          nItems);
            // `consecutiveRepeats` and its corresponding threshold are used to set an upper bound
            // on how hard we should try to pull unique values before we give up.
            int8_t consecutiveRepeats = 0;
            const int8_t consecutiveRepeatsThreshold = 100;

            while (distinctValues.size() < nItems) {
                // Use the Appendable visitor pattern to generate a value.
                bsoncxx::builder::basic::array tempBuilder{};
                _valueGen->append(tempBuilder);

                // Extract the value as an array::element and attempt to insert it into our set.
                auto [itr, succeeded] = distinctValues.insert(tempBuilder.extract());

                // Success indicates that the value is unique.
                if (succeeded) {
                    // Since there's only one element in the array, we just use the first element.
                    auto& element = *itr->view().begin();
                    // Shamelessly stolen from the CXX driver library
                    // (bsoncxx/types/bson_value/view.cpp). This will generate a case for each BSON
                    // value type, call the correct accessor function, and append it to `builder`.
                    // It would be much nicer if this were implemented with std::variant<>, so that
                    // we can use std::visit, but alas.
                    switch (static_cast<int>(element.type())) {
#define BSONCXX_ENUM(type, val)               \
    case val: {                               \
        builder.append(element.get_##type()); \
        break;                                \
    }
#include <bsoncxx/enums/type.hpp>
#undef BSONCXX_ENUM
                    }
                    // End of shameless stealing.
                    consecutiveRepeats = 0;  // Reset repetition counter.
                } else if (++consecutiveRepeats > consecutiveRepeatsThreshold) {
                    // If we hit the threshold, we just give up.
                    std::stringstream msg;
                    msg << "Repeatedly failed to find a new distinct value " << consecutiveRepeats
                        << " times. Terminating distinct array value "
                        << "generation because we are likely to hit an infinite loop. Node: "
                        << _node;
                    BOOST_THROW_EXCEPTION(ValueGeneratorConvergenceTimeout(msg.str()));
                }
            }
        } else {
            for (int i = 0; i < nItems; ++i) {
                _valueGen->append(builder);
            }
        }
        return builder.extract();
    }

private:
    DefaultRandom& _rng;
    const Node& _node;
    const GeneratorArgs& _generatorArgs;
    const UniqueAppendable _valueGen;
    const UniqueGenerator<int64_t> _nItemsGen;
    const bool _distinct;
};

class ConcatGenerator : public Generator<bsoncxx::array::value> {

public:
    ConcatGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng}, _id{generatorArgs.actorId} {
        if (!node["arrays"].isSequence()) {
            std::stringstream msg;
            msg << "Malformed node for concat arrays. Not a sequence " << node;
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
        for (const auto&& [k, v] : node["arrays"]) {
            _parts.push_back(bsonArrayGenerator(v, generatorArgs));
        }
    }

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder{};
        for (auto&& arrGen : _parts) {
            bsoncxx::array::value arr = arrGen->evaluate();
            for (auto x : arr.view()) {
                builder.append(x.get_value());
            }
        }
        auto val = builder.extract();
        return val;
    }

protected:
    DefaultRandom& _rng;
    ActorId _id;
    std::vector<UniqueGenerator<bsoncxx::array::value>> _parts;
};

/**
 * `{^Object: {withNEntries: 10, havingKeys: {^Foo}, andValues: {^Bar}, allowDuplicateKeys: bool}`
 */
class ObjectGenerator : public Generator<bsoncxx::document::value> {
public:
    enum class OnDuplicatedKeys {
        // BSON supports objects with duplicated keys and in some cases we might want to test such
        // scenarios. Allowing to insert duplicated keys is faster than tracking them, so in the
        // cases when duplicates are impossible (or unlikely and don't affect the test), this option
        // is also a good choice.
        insert = 0,
        // The configuration to skip duplicated keys tracks already inserted keys and never inserts
        // duplicates. This means that the resulting object might have fewer keys than specified in
        // 'withNEntries' setting.
        skip,
        // TODO: "retry" option, that is, regenerate the key until get a unique one.
    };
    ObjectGenerator(const Node& node,
                    GeneratorArgs generatorArgs,
                    std::map<std::string, Parser<UniqueAppendable>> parsers)
        : _rng{generatorArgs.rng},
          _node{node},
          _generatorArgs{generatorArgs},
          _keyGen{stringGenerator(node["havingKeys"], generatorArgs)},
          _valueGen{
              valueGenerator<false, UniqueAppendable>(node["andValues"], generatorArgs, parsers)},
          _nTimesGen{intGenerator(extract(node, "withNEntries", "^Object"), generatorArgs)} {
        const auto duplicatedKeys = _node["duplicatedKeys"].to<std::string>();
        if (duplicatedKeys == "insert") {
            _onDuplicatedKeys = OnDuplicatedKeys::insert;
        } else if (duplicatedKeys == "skip") {
            _onDuplicatedKeys = OnDuplicatedKeys::skip;
        } else {
            BOOST_THROW_EXCEPTION(
                InvalidValueGeneratorSyntax("Unknown value for 'duplicatedKeys'"));
        }
    }

    bsoncxx::document::value evaluate() override {
        bsoncxx::builder::basic::document builder;
        auto times = _nTimesGen->evaluate();

        std::unordered_set<std::string> usedKeys;
        for (int i = 0; i < times; ++i) {
            auto key = _keyGen->evaluate();
            if (_onDuplicatedKeys == OnDuplicatedKeys::insert || usedKeys.insert(key).second) {
                _valueGen->append(key, builder);
            }
        }
        return builder.extract();
    }

private:
    DefaultRandom& _rng;
    const Node& _node;
    const GeneratorArgs& _generatorArgs;
    const UniqueGenerator<std::string> _keyGen;
    const UniqueAppendable _valueGen;
    const UniqueGenerator<int64_t> _nTimesGen;
    OnDuplicatedKeys _onDuplicatedKeys;
};

class IncGenerator : public Generator<int64_t> {
public:
    IncGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _step{node["step"].maybe<int64_t>().value_or(1)} {
       
        int64_t start = node["start"] ? intGenerator(node["start"], generatorArgs)->evaluate() : 1;
        _counter = start +
            generatorArgs.actorId * node["multiplier"].maybe<int64_t>().value_or(0);
    }

    int64_t evaluate() override {
        auto inc_value = _counter;
        _counter += _step;
        return inc_value;
    }

private:
    int64_t _step;
    int64_t _counter;
};


class TwoDWalkGenerator : public Generator<bsoncxx::array::value> {
public:
    TwoDWalkGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng(generatorArgs.rng),
          _docsPerSeries{extract(node, "docsPerSeries", "TwoDWalk").maybe<int64_t>().value()},
          _distPerDoc{extract(node, "distPerDoc", "TwoDWalk").maybe<double>().value()},
          _genX{
              extract(node, "minX", "TwoDWalk").maybe<double>().value(),
              extract(node, "maxX", "TwoDWalk").maybe<double>().value(),
          },
          _genY{
              extract(node, "minY", "TwoDWalk").maybe<double>().value(),
              extract(node, "maxY", "TwoDWalk").maybe<double>().value(),
          } {}

    bsoncxx::array::value evaluate() override {
        if (_numGenerated % _docsPerSeries == 0) {
            beginSeries();
        }
        ++_numGenerated;

        _x += _vx;
        _y += _vy;

        bsoncxx::builder::basic::array builder{};
        builder.append(_x);
        builder.append(_y);
        return builder.extract();
    }

private:
    using Uniform = boost::random::uniform_real_distribution<double>;

    // Pick a new random position and velocity.
    void beginSeries() {
        _x = _genX(_rng);
        _y = _genY(_rng);

        double pi = acos(-1);
        double dir = Uniform{0, 2 * pi}(_rng);
        _vx = _distPerDoc * cos(dir);
        _vy = _distPerDoc * sin(dir);
    }

    DefaultRandom& _rng;

    // Initial arguments.
    const int64_t _docsPerSeries;
    const double _distPerDoc;
    const Uniform _genX;
    const Uniform _genY;

    // Mutable state.
    double _x, _y;    // position
    double _vx, _vy;  // "velocity", in units per document.
    int64_t _numGenerated{0};
};


/** `{a: [...]}` */
class LiteralArrayGenerator : public Generator<bsoncxx::array::value> {
public:
    using ValueType = std::vector<UniqueAppendable>;

    explicit LiteralArrayGenerator(ValueType values) : _values{std::move(values)} {}

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder{};
        for (auto&& value : _values) {
            value->append(builder);
        }
        return builder.extract();
    }

private:
    const ValueType _values;
};

/**
 * @tparam O
 *   parser output type
 * @param node
 *   a sub-field e.g. {^RandomInt:{...}} or a scalar.
 * @param parsers
 *   Which parsers (all of type Parser<O>) to use.
 * @return Parser<O>
 *   if the document reprents a evaluate-able structure.
 *   E.g. returns the randomInt parser if node is {^RandomInt:{...}}
 *   or nullopt if node is a scalar etc. Throws if node looks
 *   evaluate-able but e.g. has two ^-prefixed keys or if the ^-prefixed
 *   key isn't in the list of parsers.
 */
template <typename O>
std::optional<std::pair<Parser<O>, std::string>> extractKnownParser(
    const Node& node, GeneratorArgs generatorArgs, std::map<std::string, Parser<O>> parsers) {
    if (!node || !node.isMap()) {
        return std::nullopt;
    }

    auto metaKey = getMetaKey(node);
    if (!metaKey) {
        return std::nullopt;
    }

    if (auto parser = parsers.find(*metaKey); parser != parsers.end()) {
        return std::make_optional<std::pair<Parser<O>, std::string>>({parser->second, *metaKey});
    }

    std::stringstream msg;
    msg << "Unknown parser '" << *metaKey << "' in node '" << node << "'";
    BOOST_THROW_EXCEPTION(UnknownParserException(msg.str()));
}

/**
 * Main entry-point into depth-first construction.
 *
 * @tparam Verbatim if we're in a ^Verbatim node
 * @tparam Out desired Parser output type
 * @param node any type of node e.g. {a:1}, {^RandomInt:{...}}, [{a:1},{^RandomInt}]
 * @param parsers must all be of type Parser<O>
 * @return the indicated parser
 */
template <bool Verbatim, typename Out>
Out valueGenerator(const Node& node,
                   GeneratorArgs generatorArgs,
                   const std::map<std::string, Parser<Out>>& parsers) {
    if constexpr (!Verbatim) {
        if (auto parserPair = extractKnownParser(node, generatorArgs, parsers)) {
            // known parser type
            return parserPair->first(node[parserPair->second], generatorArgs);
        }
    }
    // switch-statement on node.Type() may be clearer

    if (node.isNull()) {
        return std::make_unique<ConstantAppender<bsoncxx::types::b_null>>();
    }
    if (node.isScalar()) {
        if (node.tag() != "tag:yaml.org,2002:str") {
            try {
                return std::make_unique<ConstantAppender<int32_t>>(node.to<int32_t>());
            } catch (const InvalidConversionException& e) {
            }
            try {
                return std::make_unique<ConstantAppender<int64_t>>(node.to<int64_t>());
            } catch (const InvalidConversionException& e) {
            }
            try {
                return std::make_unique<ConstantAppender<double>>(node.to<double>());
            } catch (const InvalidConversionException& e) {
            }
            try {
                return std::make_unique<ConstantAppender<bool>>(node.to<bool>());
            } catch (const InvalidConversionException& e) {
            }
        }
        return std::make_unique<ConstantAppender<std::string>>(node.to<std::string>());
    }
    if (node.isSequence()) {
        return literalArrayGenerator<Verbatim>(node, generatorArgs);
    }
    if (node.isMap()) {
        return documentGenerator<Verbatim>(node, generatorArgs);
    }

    std::stringstream msg;
    msg << "Malformed node " << node;
    BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
}

/**
 * Generate the parser maps.
 *
 * This uses a lambda to ensure that we can build the `allParsers` map safely and easily, while
 * keeping the global variable const.
 *
 * To add a new parser, add it in the corresponding parser and it will automatically be included in
 * `allParsers` as long as the key isn't already defined. Since `map.insert()` does not overwrite
 * existing keys, you can also specify separate behavior for a key by entering it in `allParser`
 * directly (e.g. "^Choose").
 *
 * If adding a new parsers map, remember to insert its entries into `allParsers` at the end of this
 * lambda function.
 */
const auto [allParsers,
            arrayParsers,
            timestampParsers,
            dateParsers,
            doubleParsers,
            intParsers,
            stringParsers,
            int32Parsers,
            decimalParsers] = []() {
    static std::map<std::string, Parser<UniqueAppendable>> allParsers{
        {"^Choose",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ChooseGenerator>(node, generatorArgs);
         }},
        {"^ObjectId",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ObjectIdGenerator>(node, generatorArgs);
         }},
        {"^UUID",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<UuidGenerator>(node, generatorArgs);
         }},
        {"^Null",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NullGenerator>(node, generatorArgs);
         }},
        {"^Verbatim",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return valueGenerator<true, UniqueAppendable>(node, generatorArgs, allParsers);
         }},
        {"^TwoDWalk",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<TwoDWalkGenerator>(node, generatorArgs);
         }},
        {"^Object",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ObjectGenerator>(node, generatorArgs, allParsers);
         }},
        {"^Cycle",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<CycleGenerator>(node, generatorArgs, allParsers);
         }},
        {"^Repeat",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<RepeatGenerator>(node, generatorArgs, allParsers);
         }},
        {"^FixedGeneratedValue",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<CycleGenerator>(node, generatorArgs, allParsers, 1);
         }},
        {"^BinData",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<BinDataGenerator>(node, generatorArgs);
         }},
        {"^BinDataSensitive",
         [](const Node& node, GeneratorArgs generatorArgs) {
             // TODO: PERF-4467 Update this to bsoncxx::binary_sub_type::sensitive.
             return std::make_unique<BinDataGenerator>(
                 node, generatorArgs, bsoncxx::binary_sub_type(0x8));
         }},
    };

    const static std::map<std::string, Parser<UniqueGenerator<bsoncxx::array::value>>> arrayParsers{
        {"^Concat",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ConcatGenerator>(node, generatorArgs);
         }},
        {"^Array",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ArrayGenerator>(node, generatorArgs, allParsers);
         }},
        {"^Verbatim",
         [](const Node& node, GeneratorArgs generatorArgs) {
             if (!node.isSequence()) {
                 std::stringstream msg;
                 msg << "Malformed node array. Not a sequence " << node;
                 BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
             }
             return literalArrayGenerator<true>(node, generatorArgs);
         }},
    };

    // Parsers to look when we request a timestamp parser
    const static std::map<std::string, Parser<UniqueGenerator<bsoncxx::types::b_timestamp>>> timestampParsers{
        {"^NowTimestamp",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NowTimestampGenerator>(node, generatorArgs);
         }},
    };

    // Set of parsers to look when we request a date parser
    const static std::map<std::string, Parser<UniqueGenerator<bsoncxx::types::b_date>>> dateParsers{
        {"^Now",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NowGenerator>(node, generatorArgs);
         }},
        {"^RandomDate",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<RandomDateGenerator>(node, generatorArgs);
         }},
        {"^Date",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<DateGenerator>(node, generatorArgs);
         }},
        {"^IncDate",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<IncDateGenerator>(node, generatorArgs);
         }},
    };

    // Set of parsers to look when we request an double parser
    // see doubleGenerator
    const static std::map<std::string, Parser<UniqueGenerator<double>>> doubleParsers{
        {"^RandomDouble", doubleGeneratorBasedOnDistribution},
        {"^ConvertToDouble",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NumericConversionGenerator<double>>(node, generatorArgs);
         }},
    };

    // Set of parsers to look when we request an int parser
    // see int64Generator
    const static std::map<std::string, Parser<UniqueGenerator<int64_t>>> intParsers{
        {"^RandomInt", int64GeneratorBasedOnDistribution},
        {"^ActorId",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ActorIdIntGenerator>(node, generatorArgs);
         }},
        // There are other things of type Generator<int64_t>. Not sure if they should be here or not
        {"^Inc",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<IncGenerator>(node, generatorArgs);
         }},
        {"^ConvertToInt",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NumericConversionGenerator<int64_t>>(node, generatorArgs);
         }},
    };

    // Set of parsers to look when we request an string parser
    // see stringGenerator
    const static std::map<std::string, Parser<UniqueGenerator<std::string>>> stringParsers{
        {"^FastRandomString",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<FastRandomStringGenerator>(node, generatorArgs);
         }},
        {"^RandomString",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NormalRandomStringGenerator>(node, generatorArgs);
         }},
        {"^ChooseFromDataset",
         [](const Node& node, GeneratorArgs generatorArgs) -> UniqueGenerator<std::string> {
            if(node["sequential"].maybe<bool>().value_or(false)) {
                return std::make_unique<ChooseStringFromDatasetSequentially>(node, generatorArgs);
            } else {
                return std::make_unique<ChooseStringFromDatasetRandomly>(node, generatorArgs);
            }
         }},
        {"^Join",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<JoinGenerator>(node, generatorArgs);
         }},
        {"^Choose",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ChooseStringGenerator>(node, generatorArgs);
         }},
        {"^TakeRandomStringFromFrequencyMap",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<FrequencyMapGenerator>(node, generatorArgs);
         }},
        {"^TakeRandomStringFromFrequencyMapSingleton",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<FrequencyMapSingletonGenerator>(node, generatorArgs);
         }},
        {"^IP",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<IPGenerator>(node, generatorArgs);
         }},
        {"^ActorIdString",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ActorIdStringGenerator>(node, generatorArgs);
         }},
        {"^FormatString",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<FormatStringGenerator>(node, generatorArgs, allParsers);
         }},
    };

    // Set of parsers to look when we request an int32 parser
    // see int32Generator
    const static std::map<std::string, Parser<UniqueGenerator<int32_t>>> int32Parsers{
        {"^ConvertToInt32",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NumericConversionGenerator<int32_t>>(node, generatorArgs);
         }},
    };

    // Set of parsers to look when we request a decimal parser
    // see decimalGenerator
    const static std::map<std::string, Parser<UniqueGenerator<bsoncxx::decimal128>>> decimalParsers{
        {"^ConvertToDecimal",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NumericConversionGenerator<bsoncxx::decimal128>>(
                 node, generatorArgs);
         }},
    };

    // Only nonexistent keys are inserted.
    allParsers.insert(arrayParsers.begin(), arrayParsers.end());
    allParsers.insert(timestampParsers.begin(), timestampParsers.end());
    allParsers.insert(dateParsers.begin(), dateParsers.end());
    allParsers.insert(doubleParsers.begin(), doubleParsers.end());
    allParsers.insert(intParsers.begin(), intParsers.end());
    allParsers.insert(stringParsers.begin(), stringParsers.end());
    allParsers.insert(int32Parsers.begin(), int32Parsers.end());
    allParsers.insert(decimalParsers.begin(), decimalParsers.end());

    return std::tie(allParsers,
                    arrayParsers,
                    timestampParsers,
                    dateParsers,
                    doubleParsers,
                    intParsers,
                    stringParsers,
                    int32Parsers,
                    decimalParsers);
}();

/**
 * Used for top-level values that are of type Map.
 * @tparam Verbatim if we are in a `^Verbatim` block
 * @param node a "top-level"-like node e.g. `{a:1, b:{^RandomInt:{...}}`
 */
template <bool Verbatim>
std::unique_ptr<DocumentGenerator::Impl> documentGenerator(const Node& node,
                                                           GeneratorArgs generatorArgs) {
    if (!node.isMap()) {
        std::ostringstream stm;
        stm << "Node " << node << " must be mapping type";
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(stm.str()));
    }
    if constexpr (!Verbatim) {
        auto meta = getMetaKey(node);
        if (meta) {
            if (meta == "^Verbatim") {
                return documentGenerator<true>(node["^Verbatim"], generatorArgs);
            }
            std::stringstream msg;
            msg << "Invalid meta-key " << *meta << " at top-level";
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
        }
    }

    DocumentGenerator::Impl::Entries entries;
    for (const auto&& [k, v] : node) {
        auto key = k.toString();
        auto valgen = valueGenerator<Verbatim, UniqueAppendable>(v, generatorArgs, allParsers);
        entries.emplace_back(key, std::move(valgen));
    }
    return std::make_unique<DocumentGenerator::Impl>(std::move(entries));
}

/**
 * @tparam Verbatim if we're in a `^Verbatim block`
 * @param node sequence node
 * @return literal array of generators that has one valueGenerator (recursive type) for each element
 * in the node
 */
template <bool Verbatim>
UniqueGenerator<bsoncxx::array::value> literalArrayGenerator(const Node& node,
                                                             GeneratorArgs generatorArgs) {
    LiteralArrayGenerator::ValueType entries;
    for (const auto&& [k, v] : node) {
        auto valgen = valueGenerator<Verbatim, UniqueAppendable>(v, generatorArgs, allParsers);
        entries.push_back(std::move(valgen));
    }
    return std::make_unique<LiteralArrayGenerator>(std::move(entries));
}

/**
 * @param node
 *   the *value* from a `^RandomDouble` node.
 *   E.g. if higher-up has `{^RandomDouble:{v}}`, this will have `node={v}`
 */
//
// We need this additional lookup function because we do "double-dispatch"
// for ^RandomDouble. So doubleOperand determines if we're looking at
// ^RandomDouble or a constant. If we're looking at ^RandomDouble
// it dispatches to here to determine which doubleGenerator to use.
//
UniqueGenerator<double> doubleGeneratorBasedOnDistribution(const Node& node,
                                                           GeneratorArgs generatorArgs) {
    if (!node.isMap()) {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("random int must be given mapping type"));
    }
    auto distribution = node["distribution"].maybe<std::string>().value_or("uniform");

    if (distribution == "uniform") {
        return std::make_unique<UniformDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "exponential") {
        return std::make_unique<ExponentialDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "gamma") {
        return std::make_unique<GammaDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "weibull") {
        return std::make_unique<WeibullDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "extreme_value") {
        return std::make_unique<ExtremeValueDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "beta") {
        return std::make_unique<BetaDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "laplace") {
        return std::make_unique<LaplaceDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "normal") {
        return std::make_unique<NormalDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "lognormal") {
        return std::make_unique<LognormalDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "chi_squared") {
        return std::make_unique<ChiSquaredDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "non_central_chi_squared") {
        return std::make_unique<NonCentralChiSquaredDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "cauchy") {
        return std::make_unique<CauchyDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "fisher_f") {
        return std::make_unique<FisherFDoubleGenerator>(node, generatorArgs);
    } else if (distribution == "student_t") {
        return std::make_unique<StudentTDoubleGenerator>(node, generatorArgs);
    } else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(error.str()));
    }
}

/**
 * @param node
 *   the *value* from a `^RandomInt` node.
 *   E.g. if higher-up has `{^RandomInt:{v}}`, this will have `node={v}`
 */
//
// We need this additional lookup function for int64s (but not for other types)
// because we do "double-dispatch" for ^RandomInt. So int64Operand determines
// if we're looking at ^RandomInt or a constant. If we're looking at ^RandomInt
// it dispatches to here to determine which Int64Generator to use.
//
// An alternative would have been to have ^RandomIntUniform etc.
//
UniqueGenerator<int64_t> int64GeneratorBasedOnDistribution(const Node& node,
                                                           GeneratorArgs generatorArgs) {
    if (!node.isMap()) {
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("random int must be given mapping type"));
    }
    auto distribution = node["distribution"].maybe<std::string>().value_or("uniform");

    if (distribution == "uniform") {
        return std::make_unique<UniformInt64Generator>(node, generatorArgs);
    } else if (distribution == "binomial") {
        return std::make_unique<BinomialInt64Generator>(node, generatorArgs);
    } else if (distribution == "negative_binomial") {
        return std::make_unique<NegativeBinomialInt64Generator>(node, generatorArgs);
    } else if (distribution == "poisson") {
        return std::make_unique<PoissonInt64Generator>(node, generatorArgs);
    } else if (distribution == "geometric") {
        return std::make_unique<GeometricInt64Generator>(node, generatorArgs);
    } else if (distribution == "zipfian") {
        return std::make_unique<ZipfianInt64Generator>(node, generatorArgs);
    } else {
        std::stringstream error;
        error << "Unknown distribution '" << distribution << "'";
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(error.str()));
    }
}


/**
 * @param node
 *   a top-level document value i.e. either a scalar or a `^RandomInt` value
 * @return
 *   either a `^RantomInt` generator (etc--see `intParsers`)
 *   or a constant generator if given a constant/scalar.
 */
UniqueGenerator<int64_t> intGenerator(const Node& node, GeneratorArgs generatorArgs) {
    if (auto parserPair = extractKnownParser(node, generatorArgs, intParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    return std::make_unique<ConstantAppender<int64_t>>(node.to<int64_t>());
}

/**
 * @param node
 *   a top-level document value i.e. either a scalar or a `^RandomDouble` value
 * @return
 *   either a `^RandomDouble` generator (etc--see `doubleParsers`)
 *   or a constant generator if given a constant/scalar.
 */
UniqueGenerator<double> doubleGenerator(const Node& node, GeneratorArgs generatorArgs) {
    if (auto parserPair = extractKnownParser(node, generatorArgs, doubleParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    return std::make_unique<ConstantAppender<double>>(node.to<double>());
}

/**
 * @param node
 *   a top-level document value i.e. either a scalar or a `^String` value
 * @return
 *   either a `^String` generator (etc--see `intParsers`)
 *   or a constant generator if given a constant/scalar.
 */
UniqueGenerator<std::string> stringGenerator(const Node& node, GeneratorArgs generatorArgs) {
    if (auto parserPair = extractKnownParser(node, generatorArgs, stringParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    return std::make_unique<ConstantAppender<std::string>>(node.to<std::string>());
}

/**
 * @param node
 *   a top-level document value i.e. either a scalar or an int32 generator value
 * @return
 *   either a `^ConvertToInt32` generator (etc--see `int32Parsers`)
 *   or a constant generator if given a constant/scalar.
 */
UniqueGenerator<int32_t> int32Generator(const Node& node, GeneratorArgs generatorArgs) {
    if (auto parserPair = extractKnownParser(node, generatorArgs, int32Parsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    return std::make_unique<ConstantAppender<int32_t>>(node.to<int32_t>());
}

/**
 * @param node
 *   Any node type that evaluates to an array (array literal, literal array of generators, ^Concat,
 * ^Array)
 * @return
 *   The proper generator based on the parsed input.
 */
UniqueGenerator<bsoncxx::array::value> bsonArrayGenerator(const Node& node,
                                                          GeneratorArgs generatorArgs) {
    if (auto parserPair = extractKnownParser(node, generatorArgs, arrayParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    if (!node.isSequence()) {
        std::stringstream msg;
        msg << "Malformed node array. Not a sequence " << node;
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
    }
    return literalArrayGenerator<false>(node, generatorArgs);
}

ChooseGenerator::ChooseGenerator(const Node& node, GeneratorArgs generatorArgs)
    : _rng{generatorArgs.rng}, _id{generatorArgs.actorId} {
    if (!node["from"].isSequence()) {
        std::stringstream msg;
        msg << "Malformed node for choose from array. Not a sequence " << node;
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
    }
    if (node["deterministic"] && node["weights"]) {
        std::stringstream msg;
        msg << "Invalid Syntax for choose: cannot have both 'deterministic' and 'weights'";
        BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax(msg.str()));
    }
    for (const auto&& [k, v] : node["from"]) {
        _choices.push_back(valueGenerator<false, UniqueAppendable>(v, generatorArgs, allParsers));
    }
    if (node["weights"]) {
        for (const auto&& [k, v] : node["weights"]) {
            _weights.push_back(v.to<int64_t>());
        }
    } else {
        // If not passed in, give each choice equal weight
        _weights.assign(_choices.size(), 1);
    }
    if (node["deterministic"]) {
        _deterministic = node["deterministic"].maybe<bool>().value_or(false);
    } else {
        _deterministic = false;
    }
    _elemNumber = -1;
}

/**
 * @private
 * @param datetime
 *   a datetime string in a known format or unsigned long long string.
 * @return
 *   The datetime as millis.
 */
int64_t parseStringToMillis(const std::string& datetime) {
    if (!datetime.empty()) {
        // TODO: PERF-2153 needs some investigation.
        for (const auto& format : formats) {
            std::istringstream date_stream{datetime};
            date_stream.imbue(format);
            boost::local_time::local_date_time local_date{boost::local_time::not_a_date_time};
            if (date_stream >> local_date) {
                return (local_date.utc_time() - epoch).total_milliseconds();
            }
        }
    }
    try {
        // Last gasp try to interpret unsigned long long.
        return std::stoull(datetime);
    } catch (const std::invalid_argument& _) {
        auto msg = "Invalid Dateformat '" + datetime + "'";
        BOOST_THROW_EXCEPTION(InvalidDateFormat{msg});
    }
};

/**
 * @private
 * @param node
 *   a node string value i.e. either a scalar or a `^String` value
 * @return
 *   a constant generator if given a constant/scalar.
 */
UniqueGenerator<int64_t> dateStringTimeGenerator(const Node& node, GeneratorArgs generatorArgs) {
    auto generator = stringGenerator(node, generatorArgs);
    return std::make_unique<DateStringParserGenerator>(std::move(generator));
}

/**
 * @private
 * @param node
 *   a node double value i.e. either a scalar or a `^RandomDouble` value
 * @return
 *   a constant generator if given a constant/scalar.
 */
UniqueGenerator<int64_t> doubleTimeGenerator(const Node& node, GeneratorArgs generatorArgs) {
    auto generator = doubleGenerator(node, generatorArgs);
    return std::make_unique<CastDoubleToInt64Generator>(std::move(generator));
}

/**
 * @private
 * @param node
 *   a node containing a ^Now or ^RandomDate.
 * @return
 *   a constant generator if given a constant/scalar.
 * @throws
 *   InvalidConfigurationException if the node doesn't hold a ^Now or ^RandomDate.
 */
UniqueGenerator<int64_t> dateTimeGenerator(const Node& node, GeneratorArgs generatorArgs) {
    if (auto parserPair = extractKnownParser(node, generatorArgs, dateParsers)) {
        // known parser type
        auto generator = parserPair->first(node[parserPair->second], generatorArgs);
        return std::make_unique<DateToIntGenerator>(std::move(generator));
    }
    // This is an internal private function, we have no sane way to interpret a scalar here,
    // so throw an exception.
    std::stringstream msg;
    msg << "Unknown parser: not an expected type " << node;
    BOOST_THROW_EXCEPTION(UnknownParserException(msg.str()));
}

/**
 * @param node
 *   a node containing a date generator.
 * generator
 * @return
 *   a constant generator if given a constant/scalar.
 */
UniqueGenerator<int64_t> dateGenerator(const Node& node,
                                       GeneratorArgs generatorArgs,
                                       const boost::posix_time::ptime& defaultTime) {
    const static auto timeGenerators = {
        dateTimeGenerator, dateStringTimeGenerator, intGenerator, doubleTimeGenerator};
    if (node) {
        for (auto&& generator : timeGenerators) {
            try {
                // InvalidValueGeneratorSyntax means that we found a parser but there was something
                // wrong, so this exception gets passed up.
                return generator(node, generatorArgs);
            } catch (const UnknownParserException& e) {
            } catch (const InvalidDateFormat& e) {
            } catch (const InvalidConversionException& e) {
            }
        }
        std::stringstream msg;
        msg << "Unknown parser: not an expected type " << node;
        BOOST_THROW_EXCEPTION(UnknownParserException(msg.str()));
    }

    // No value, get the appropriate default.
    auto millis = (defaultTime - epoch).total_milliseconds();
    return std::make_unique<ConstantAppender<int64_t>>(millis);
}
}  // namespace

// Kick the recursion into motion
DocumentGenerator::DocumentGenerator(const Node& node, GeneratorArgs generatorArgs)
    : _impl{documentGenerator<false>(node, generatorArgs)} {}
DocumentGenerator::DocumentGenerator(const Node& node, PhaseContext& phaseContext, ActorId actorId)
    : DocumentGenerator{node, GeneratorArgs{phaseContext.rng(actorId), actorId}} {}
DocumentGenerator::DocumentGenerator(const Node& node, ActorContext& actorContext, ActorId actorId)
    : DocumentGenerator{node, GeneratorArgs{actorContext.rng(actorId), actorId}} {}


DocumentGenerator::DocumentGenerator(DocumentGenerator&&) noexcept = default;

DocumentGenerator::~DocumentGenerator() = default;

// Can't define this before DocumentGenerator::Impl ↑
bsoncxx::document::value DocumentGenerator::operator()() {
    return _impl->evaluate();
}

bsoncxx::document::value DocumentGenerator::evaluate() {
    return operator()();
}

namespace genny {
// template <class T>
// TypeGenerator<T>::TypeGenerator(const Node& node, GeneratorArgs generatorArgs) {}
template <class T>
T TypeGenerator<T>::evaluate() {
    return (_impl->evaluate());
}
TypeGenerator<int64_t> makeIntGenerator(const Node& node, GeneratorArgs generatorArgs) {
    return (TypeGenerator<int64_t>(std::move(intGenerator(node, generatorArgs))));
}
TypeGenerator<double> makeDoubleGenerator(const Node& node, GeneratorArgs generatorArgs) {
    return (TypeGenerator<double>(std::move(doubleGenerator(node, generatorArgs))));
}

template <class T>
TypeGenerator<T>::~TypeGenerator() = default;
template <class T>
TypeGenerator<T>::TypeGenerator(TypeGenerator<T>&&) noexcept = default;
template <class T>
TypeGenerator<T>& TypeGenerator<T>::operator=(TypeGenerator<T>&&) noexcept = default;

// Force the compiler to build the following TypeGenerators
template class TypeGenerator<double>;
template class TypeGenerator<int64_t>;
}  // namespace genny
