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

#include <value_generators/DocumentGenerator.hpp>

#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <boost/algorithm/string/join.hpp>
#include <boost/date_time.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types/bson_value/view.hpp>


namespace {
using bsoncxx::oid;

class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
    virtual void append(bsoncxx::builder::basic::array& builder) = 0;
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
            BOOST_THROW_EXCEPTION(InvalidValueGeneratorSyntax("Found multiple meta-keys"));
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

// This generator allows choosing any valid generator, incuding documents. As such it cannot be used
// by JoinGenerator today. See ChooseStringGenerator.
class ChooseGenerator : public Appendable {
public:
    // constructore defined at bottom of the file to use other symbol
    ChooseGenerator(const Node& node, GeneratorArgs generatorArgs);
    Appendable& choose() {
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
};


// This is a a more specific version of ChooseGenerator that produces strings. It is only used
// within the JoinGenerator.
class ChooseStringGenerator : public Generator<std::string> {
public:
    ChooseStringGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _rng{generatorArgs.rng}, _id{generatorArgs.actorId} {
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
    }
    std::string evaluate() override {
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
};

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
            message % val.get_utf8().value.to_string();
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

/** `{^Now: {}}` */
class NowGenerator : public Generator<bsoncxx::types::b_date> {
public:
    NowGenerator(const Node& node, GeneratorArgs generatorArgs) {}
    bsoncxx::types::b_date evaluate() override {
        return bsoncxx::types::b_date{std::chrono::system_clock::now()};
    }
};

// The 2 formats also cover "%Y-%m-%d". Timezones require local_time_input_facet AND local_date_time
// see https://www.boost.org/doc/libs/1_75_0/doc/html/date_time/date_time_io.html.
// We strive to use smart pointers where possible. In this case this is not possible
// but not a huge deal as these objects are statically allocated.
const static auto formats = {
    std::locale(std::locale::classic(),
                new boost::local_time::local_time_input_facet("%Y-%m-%dT%H:%M:%s%ZP")),
    std::locale(std::locale::classic(),
                new boost::local_time::local_time_input_facet("%Y-%m-%d %H:%M:%s%ZP")),
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
          _nTimesGen{intGenerator(extract(node, "number", "^Array"), generatorArgs)} {}

    bsoncxx::array::value evaluate() override {
        bsoncxx::builder::basic::array builder{};
        auto times = _nTimesGen->evaluate();
        for (int i = 0; i < times; ++i) {
            _valueGen->append(builder);
        }
        return builder.extract();
    }

private:
    DefaultRandom& _rng;
    const Node& _node;
    const GeneratorArgs& _generatorArgs;
    const UniqueAppendable _valueGen;
    const UniqueGenerator<int64_t> _nTimesGen;
};


class IncGenerator : public Generator<int64_t> {
public:
    IncGenerator(const Node& node, GeneratorArgs generatorArgs)
        : _step{node["step"].maybe<int64_t>().value_or(1)} {
        _counter = node["start"].maybe<int64_t>().value_or(1) +
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
        if (node.tag() != "!") {
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

const static std::map<std::string, Parser<UniqueAppendable>> allParsers{
    {"^FastRandomString",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<FastRandomStringGenerator>(node, generatorArgs);
     }},
    {"^RandomString",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<NormalRandomStringGenerator>(node, generatorArgs);
     }},
    {"^Join",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<JoinGenerator>(node, generatorArgs);
     }},
    {"^FormatString",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<FormatStringGenerator>(node, generatorArgs, allParsers);
     }},
    {"^Choose",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<ChooseGenerator>(node, generatorArgs);
     }},
    {"^IP",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<IPGenerator>(node, generatorArgs);
     }},
    {"^ActorIdString",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<ActorIdStringGenerator>(node, generatorArgs);
     }},
    {"^ActorId",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<ActorIdIntGenerator>(node, generatorArgs);
     }},
    {"^RandomInt", int64GeneratorBasedOnDistribution},
    {"^RandomDouble", doubleGeneratorBasedOnDistribution},
    {"^Now",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<NowGenerator>(node, generatorArgs);
     }},
    {"^RandomDate",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<RandomDateGenerator>(node, generatorArgs);
     }},
    {"^ObjectId",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<ObjectIdGenerator>(node, generatorArgs);
     }},
    {"^Verbatim",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return valueGenerator<true, UniqueAppendable>(node, generatorArgs, allParsers);
     }},
    {"^Inc",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<IncGenerator>(node, generatorArgs);
     }},
    {"^Array",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<ArrayGenerator>(node, generatorArgs, allParsers);
     }},
    {"^Cycle",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<CycleGenerator>(node, generatorArgs, allParsers);
     }},
    {"^FixedGeneratedValue",
     [](const Node& node, GeneratorArgs generatorArgs) {
         return std::make_unique<CycleGenerator>(node, generatorArgs, allParsers, 1);
     }},
};


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
    // Set of parsers to look when we request an int parser
    // see int64Generator
    const static std::map<std::string, Parser<UniqueGenerator<int64_t>>> intParsers{
        {"^RandomInt", int64GeneratorBasedOnDistribution},
        {"^ActorId",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ActorIdIntGenerator>(node, generatorArgs);
         }},
    };

    if (auto parserPair = extractKnownParser(node, generatorArgs, intParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    return std::make_unique<ConstantAppender<int64_t>>(node.to<int64_t>());
}

/**
 * @param node
 *   a top-level document value i.e. either a scalar or a `^RandomInt` value
 * @return
 *   either a `^RantomInt` generator (etc--see `intParsers`)
 *   or a constant generator if given a constant/scalar.
 */
UniqueGenerator<double> doubleGenerator(const Node& node, GeneratorArgs generatorArgs) {
    // Set of parsers to look when we request an double parser
    // see doubleGenerator
    const static std::map<std::string, Parser<UniqueGenerator<double>>> doubleParsers{
        {"^RandomDouble", doubleGeneratorBasedOnDistribution},
    };

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
    // Set of parsers to look when we request an int parser
    // see int64Generator
    const static std::map<std::string, Parser<UniqueGenerator<std::string>>> stringParsers{
        {"^FastRandomString",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<FastRandomStringGenerator>(node, generatorArgs);
         }},
        {"^RandomString",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<NormalRandomStringGenerator>(node, generatorArgs);
         }},
        {"^Join",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<JoinGenerator>(node, generatorArgs);
         }},
        {"^Choose",
         [](const Node& node, GeneratorArgs generatorArgs) {
             return std::make_unique<ChooseStringGenerator>(node, generatorArgs);
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

    if (auto parserPair = extractKnownParser(node, generatorArgs, stringParsers)) {
        // known parser type
        return parserPair->first(node[parserPair->second], generatorArgs);
    }
    return std::make_unique<ConstantAppender<std::string>>(node.to<std::string>());
}

ChooseGenerator::ChooseGenerator(const Node& node, GeneratorArgs generatorArgs)
    : _rng{generatorArgs.rng}, _id{generatorArgs.actorId} {
    if (!node["from"].isSequence()) {
        std::stringstream msg;
        msg << "Malformed node for choose from array. Not a sequence " << node;
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
        auto msg = "^RandomDate: Invalid Dateformat '" + datetime + "'";
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
    };

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
