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

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <testlib/helpers.hpp>
#include <value_generators/DocumentGenerator.hpp>

namespace genny::v1 {
namespace {

namespace BasicBson = bsoncxx::builder::basic;
namespace BsonTypes = bsoncxx::types;

void assert_documents_equal(bsoncxx::document::view expected, bsoncxx::document::view actual) {
    REQUIRE(bsoncxx::to_json(expected, bsoncxx::ExtendedJsonMode::k_canonical) ==
            bsoncxx::to_json(actual, bsoncxx::ExtendedJsonMode::k_canonical));
}

void assert_arrays_equal(bsoncxx::array::view expected, bsoncxx::array::view actual) {
    REQUIRE(bsoncxx::to_json(expected, bsoncxx::ExtendedJsonMode::k_canonical) ==
            bsoncxx::to_json(actual, bsoncxx::ExtendedJsonMode::k_canonical));
}

TEST_CASE("Expression parsing with Parameter always errors") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    auto yaml = YAML::Load(R"({^Parameter: {Default: "Required", Name: "Required"}})");
    REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
}

TEST_CASE("Expression::parseExpression error cases") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);
    }

    SECTION("must be a mapping type") {
        auto yaml = YAML::Load(R"(
scalarValue
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
null
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
[sequence, value]
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
[]
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("must have exactly one key") {
        auto yaml = YAML::Load(R"(
{extraKeyBefore: 1, ^RandomInt: {min: 50, max: 60}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}, extraKeyAfter: 1}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("must be a known expression type") {
        auto yaml = YAML::Load(R"(
{RandomInt: {min: 50, max: 60}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^NonExistent: {min: 50, max: 60}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression::parseObject error cases") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}}
        )");

        auto expr = Expression::parseObject(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
{}
        )");

        expr = Expression::parseObject(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
{RandomInt: {min: 50, max: 60}}
        )");

        expr = Expression::parseObject(yaml, rng);
        REQUIRE(expr != nullptr);
    }

    SECTION("must be a mapping type") {
        auto yaml = YAML::Load(R"(
scalarValue
        )");

        REQUIRE_THROWS_AS(Expression::parseObject(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
null
        )");

        REQUIRE_THROWS_AS(Expression::parseObject(yaml, rng), InvalidValueGeneratorSyntax);


        yaml = YAML::Load(R"(
[sequence, value]
        )");

        REQUIRE_THROWS_AS(Expression::parseObject(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
[]
        )");

        REQUIRE_THROWS_AS(Expression::parseObject(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("must not mix '^' and non-'^' prefixed keys") {
        auto yaml = YAML::Load(R"(
{otherKey: 1, ^RandomInt: {min: 50, max: 60}}
        )");

        REQUIRE_THROWS_AS(Expression::parseObject(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}, otherKey: 1}
        )");

        REQUIRE_THROWS_AS(Expression::parseObject(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression::parseOperand error cases") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    SECTION("Document with no templates") {
        auto yaml = YAML::Load(R"({a: 1})");
        auto expr = Expression::parseOperand(yaml, rng);
        assert_documents_equal(
            expr->evaluate(rng).getDocument(),
            BasicBson::make_document(BasicBson::kvp("a", BsonTypes::b_int32{1})));
    }


    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
{min: 50, max: 60}
        )");

        auto expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}}
        )");

        expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
{}
        )");

        expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
scalarValue
        )");

        expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
null
        )");

        expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
[sequence, value]
        )");

        expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);

        yaml = YAML::Load(R"(
[]
        )");

        expr = Expression::parseOperand(yaml, rng);
        REQUIRE(expr != nullptr);
    }

    SECTION("must be defined") {
        auto yaml = YAML::Load(R"(
{}
        )")["nonExistent"];

        REQUIRE_THROWS_AS(Expression::parseOperand(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression parsing with ConstantExpression::parse") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    SECTION("type errors caught at parse-time") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {min: [7], max: 100}}
        )");
        REQUIRE_THROWS(Expression::parseExpression(yaml, rng));
    }

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
1
        )");

        auto expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getInt32() == 1);

        yaml = YAML::Load(R"(
269849313357703264
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getInt64() == 269849313357703264LL);

        yaml = YAML::Load(R"(
3.14
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getDouble() == 3.14);

        yaml = YAML::Load(R"(
string
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getString() == "string");

        yaml = YAML::Load(R"(
'5'
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getString() == "5");

        yaml = YAML::Load(R"(
null
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getNull() == BsonTypes::b_null{});
    }

    SECTION("valid syntax for boolean values") {
        auto yaml = YAML::Load(R"(
true
            )");

        auto expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getBool() == true);

        yaml = YAML::Load(R"(
false
            )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getBool() == false);

        yaml = YAML::Load(R"(
on
            )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getBool() == true);

        yaml = YAML::Load(R"(
off
            )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getBool() == false);

        yaml = YAML::Load(R"(
yes
            )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getBool() == true);

        yaml = YAML::Load(R"(
no
            )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        REQUIRE(expr->evaluate(rng).getBool() == false);
    }

    SECTION("valid syntax for literal objects") {
        auto yaml = YAML::Load(R"(
{min: 50, max: 60}
        )");

        auto expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);

        assert_documents_equal(
            expr->evaluate(rng).getDocument(),
            BasicBson::make_document(BasicBson::kvp("min", BsonTypes::b_int32{50}),
                                     BasicBson::kvp("max", BsonTypes::b_int32{60})));

        yaml = YAML::Load(R"(
{}
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);

        assert_documents_equal(expr->evaluate(rng).getDocument(), BasicBson::make_document());
    }

    SECTION("valid syntax for literal arrays") {
        auto yaml = YAML::Load(R"(
[sequence, value]
        )");

        auto expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);

        assert_arrays_equal(expr->evaluate(rng).getArray(),
                            BasicBson::make_array("sequence", "value"));

        yaml = YAML::Load(R"(
[]
        )");

        expr = ConstantExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);

        assert_arrays_equal(expr->evaluate(rng).getArray(), BasicBson::make_array());
    }
}

TEST_CASE("Expression parsing with DocumentExpression::parse") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
{min: 50, max: 60}
        )");

        auto expr = DocumentExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_documents_equal(
            expr->evaluate(rng).getDocument(),
            BasicBson::make_document(BasicBson::kvp("min", BsonTypes::b_int32{50}),
                                     BasicBson::kvp("max", BsonTypes::b_int32{60})));

        yaml = YAML::Load(R"(
{}
        )");

        expr = DocumentExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_documents_equal(expr->evaluate(rng).getDocument(), BasicBson::make_document());
    }

    SECTION("must be a mapping type") {
        auto yaml = YAML::Load(R"(
scalarValue
        )");

        REQUIRE_THROWS_AS(DocumentExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
[sequence, value]
        )");

        REQUIRE_THROWS_AS(DocumentExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
[]
        )");

        REQUIRE_THROWS_AS(DocumentExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("must not be an expression") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}}
        )");

        REQUIRE_THROWS_AS(DocumentExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{otherKey: 1, ^RandomInt: {min: 50, max: 60}}
        )");

        REQUIRE_THROWS_AS(DocumentExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {min: 50, max: 60}, otherKey: 1}
        )");

        REQUIRE_THROWS_AS(DocumentExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression parsing with ArrayExpression::parse") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
[sequence, type]
        )");

        auto expr = ArrayExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_arrays_equal(expr->evaluate(rng).getArray(),
                            BasicBson::make_array("sequence", "type"));

        yaml = YAML::Load(R"(
[]
        )");

        expr = ArrayExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_arrays_equal(expr->evaluate(rng).getArray(), BasicBson::make_array());
    }

    SECTION("valid syntax for heterogeneous elements") {
        auto yaml = YAML::Load(R"(
[1, 269849313357703264, 3.14, string, true, null]
        )");

        auto expr = ArrayExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_arrays_equal(expr->evaluate(rng).getArray(),
                            BasicBson::make_array(BsonTypes::b_int32{1},
                                                  BsonTypes::b_int64{269849313357703264LL},
                                                  BsonTypes::b_double{3.14},
                                                  "string",
                                                  BsonTypes::b_bool{true},
                                                  BsonTypes::b_null{}));
    }

    SECTION("valid syntax for multiple expression elements") {
        auto yaml = YAML::Load(R"(
- {^RandomInt: {min: 10, max: 10}}
- {^RandomInt: {min: 10, max: 10}}
- 10
        )");

        auto expr = ArrayExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_arrays_equal(expr->evaluate(rng).getArray(),
                            BasicBson::make_array(BsonTypes::b_int64{10},
                                                  BsonTypes::b_int64{10},
                                                  BsonTypes::b_int32{10}));

        yaml = YAML::Load(R"(
- {^RandomInt: {min: 20, max: {^RandomInt: {min: 20, max: 20}}}}
- {^RandomInt: {min: {^RandomInt: {min: 20, max: 20}}, max: 20}}
- 20
        )");

        expr = ArrayExpression::parse(yaml, rng);
        REQUIRE(expr != nullptr);
        assert_arrays_equal(expr->evaluate(rng).getArray(),
                            BasicBson::make_array(BsonTypes::b_int64{20},
                                                  BsonTypes::b_int64{20},
                                                  BsonTypes::b_int32{20}));
    }

    SECTION("must be a sequence type") {
        auto yaml = YAML::Load(R"(
scalarValue
        )");

        REQUIRE_THROWS_AS(ArrayExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{min: 50, max: 60}
        )");

        REQUIRE_THROWS_AS(ArrayExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{}
        )");

        REQUIRE_THROWS_AS(ArrayExpression::parse(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression parsing with RandomIntExpression") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    // We call Expression::evaluate() multiple times on the same Expression instance to verify
    // nothing goes wrong. The method is currently marked const but that could change in the future
    // as could the introduction of `mutable` members for caching purposes.
    const int kNumSamples = 10;

    SECTION("valid syntax for uniform distribution") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: uniform, min: 50, max: 60}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getInt64();
            REQUIRE(value >= 50);
            REQUIRE(value <= 60);
        }
    }

    SECTION("uniform distribution requires both 'min' and 'max' parameters") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: uniform, min: 50}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: uniform, max: 60}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: uniform}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("uniform distribution requires integer 'min' and 'max' parameters") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: uniform, min: 50.0, max: 60}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: uniform, min: 50, max: 60.0}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("binomial distribution") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: binomial, t: 100, p: 0.05}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getInt64();
            REQUIRE(value >= 0);
            REQUIRE(value <= 100);
        }
    }

    SECTION("binomial distribution requires both 't' and 'p' parameters") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: binomial, t: 100}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: binomial, p: 0.05}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: binomial}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("binomial distribution requires integer 't' parameter") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: binomial, t: 100.0, p: 0.05}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("negative_binomial distribution") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: negative_binomial, k: 100, p: 0.95}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getInt64();
            REQUIRE(value >= 0);
        }
    }

    SECTION("negative_binomial distribution requires both 'k' and 'p' parameters") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: negative_binomial, k: 100}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: negative_binomial, p: 0.95}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomInt: {distribution: negative_binomial}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("negative_binomial distribution requires integer 'k' parameter") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: negative_binomial, k: 100.0, p: 0.95}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("negative_binomial distribution requires double 'p' parameter") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: negative_binomial, k: 100.0, p: 9}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("geometric distribution") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: geometric, p: 0.05}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getInt64();
            REQUIRE(value >= 0);
        }
    }

    SECTION("geometric distribution requires a 'p' parameter") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: geometric}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("poisson distribution") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: poisson, mean: 5.6}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getInt64();
            REQUIRE(value >= 0);
        }
    }

    SECTION("poisson distribution requires a 'mean' parameter") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: poisson}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("requires known distribution") {
        auto yaml = YAML::Load(R"(
{^RandomInt: {distribution: non_existent}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression parsing with RandomStringExpression") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    // We call Expression::evaluate() multiple times on the same Expression instance to verify
    // nothing goes wrong. The method is currently marked const but that could change in the future
    // as could the introduction of `mutable` members for caching purposes.
    const int kNumSamples = 10;

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
{^RandomString: {length: 15}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getString();
            INFO("Generated string: " << value);
            REQUIRE(value.size() == 15);
        }

        yaml = YAML::Load(R"(
{^RandomString: {length: 15, alphabet: xyz}}
        )");

        expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getString();
            INFO("Generated string: " << value);
            REQUIRE(value.size() == 15);
            for (auto&& c : value) {
                REQUIRE((c == 'x' || c == 'y' || c == 'z'));
            }
        }

        yaml = YAML::Load(R"(
{^RandomString: {length: 15, alphabet: x}}
        )");

        expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getString();
            REQUIRE(value == std::string(15, 'x'));
        }
    }

    SECTION("requires 'length' parameter") {
        auto yaml = YAML::Load(R"(
{^RandomString: {}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);

        yaml = YAML::Load(R"(
{^RandomString: {alphabet: abc}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }

    SECTION("requires non-empty 'alphabet' parameter if specified") {
        auto yaml = YAML::Load(R"(
{^RandomString: {length: 15, alphabet: ''}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression parsing with FastRandomStringExpression") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    // We call Expression::evaluate() multiple times on the same Expression instance to verify
    // nothing goes wrong. The method is currently marked const but that could change in the future
    // as could the introduction of `mutable` members for caching purposes.
    const int kNumSamples = 10;

    SECTION("valid syntax") {
        auto yaml = YAML::Load(R"(
{^FastRandomString: {length: 15}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            auto value = expr->evaluate(rng).getString();
            INFO("Generated string: " << value);
            REQUIRE(value.size() == 15);
        }
    }

    SECTION("requires 'length' parameter") {
        auto yaml = YAML::Load(R"(
{^FastRandomString: {}}
        )");

        REQUIRE_THROWS_AS(Expression::parseExpression(yaml, rng), InvalidValueGeneratorSyntax);
    }
}

TEST_CASE("Expression parsing with ConstantExpression") {
    genny::DefaultRandom rng{};
    rng.seed(269849313357703264LL);

    // We call Expression::evaluate() multiple times on the same Expression instance to verify
    // nothing goes wrong. The method is currently marked const but that could change in the future
    // as could the introduction of `mutable` members for caching purposes.
    const int kNumSamples = 10;

    SECTION("valid syntax for literal objects") {
        auto yaml = YAML::Load(R"(
{^Verbatim: {^RandomInt: {min: 50, max: 60}}}
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            assert_documents_equal(
                expr->evaluate(rng).getDocument(),
                BasicBson::make_document(BasicBson::kvp(
                    "^RandomInt",
                    BasicBson::make_document(BasicBson::kvp("min", BsonTypes::b_int32{50}),
                                             BasicBson::kvp("max", BsonTypes::b_int32{60})))));
        }

        yaml = YAML::Load(R"(
{^Verbatim: {otherKey: 1, ^RandomInt: {min: 50, max: 60}}}
        )");

        expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            assert_documents_equal(
                expr->evaluate(rng).getDocument(),
                BasicBson::make_document(
                    BasicBson::kvp("otherKey", BsonTypes::b_int32{1}),
                    BasicBson::kvp(
                        "^RandomInt",
                        BasicBson::make_document(BasicBson::kvp("min", BsonTypes::b_int32{50}),
                                                 BasicBson::kvp("max", BsonTypes::b_int32{60})))));
        }

        yaml = YAML::Load(R"(
{^Verbatim: {^RandomInt: {min: 50, max: 60}, otherKey: 1}}
        )");

        expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            assert_documents_equal(
                expr->evaluate(rng).getDocument(),
                BasicBson::make_document(
                    BasicBson::kvp(
                        "^RandomInt",
                        BasicBson::make_document(BasicBson::kvp("min", BsonTypes::b_int32{50}),
                                                 BasicBson::kvp("max", BsonTypes::b_int32{60}))),
                    BasicBson::kvp("otherKey", BsonTypes::b_int32{1})));
        }

        yaml = YAML::Load(R"(
{^Verbatim: {^RandomString: {length: 15}}}
        )");

        expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            assert_documents_equal(
                expr->evaluate(rng).getDocument(),
                BasicBson::make_document(BasicBson::kvp(
                    "^RandomString",
                    BasicBson::make_document(BasicBson::kvp("length", BsonTypes::b_int32{15})))));
        }
    }

    SECTION("valid syntax for literal arrays") {
        auto yaml = YAML::Load(R"(
^Verbatim:
- ^RandomInt: {min: 50, max: 60}
- ^RandomString: {length: 15}
- scalarValue
        )");

        auto expr = Expression::parseExpression(yaml, rng);
        REQUIRE(expr != nullptr);

        for (int i = 0; i < kNumSamples; ++i) {
            assert_arrays_equal(
                expr->evaluate(rng).getArray(),
                BasicBson::make_array(
                    BasicBson::make_document(BasicBson::kvp(
                        "^RandomInt",
                        BasicBson::make_document(BasicBson::kvp("min", BsonTypes::b_int32{50}),
                                                 BasicBson::kvp("max", BsonTypes::b_int32{60})))),
                    BasicBson::make_document(
                        BasicBson::kvp("^RandomString",
                                       BasicBson::make_document(
                                           BasicBson::kvp("length", BsonTypes::b_int32{15})))),
                    "scalarValue"));
        }
    }
}

}  // namespace
}  // namespace genny::v1
