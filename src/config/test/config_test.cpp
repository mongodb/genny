#include <config/config.hpp>

#include <catch2/catch.hpp>

#include <map>
#include <vector>

using namespace config;

struct EmptyStruct {};

struct ExtractsStringMsgOrTakesStringCtor {
    std::string msg;
    ExtractsStringMsgOrTakesStringCtor(std::string msg)
    : msg{msg} {}
    ExtractsStringMsgOrTakesStringCtor(const Node& node)
    : ExtractsStringMsgOrTakesStringCtor(node["msg"].to<std::string>()) {}
};

struct TakesEmptyStructAndExtractsMsg {
    std::string msg;
    TakesEmptyStructAndExtractsMsg(const Node& node, EmptyStruct*)
        : msg{node["msg"].to<std::string>()} {}
};

struct RequiresParamToEqualNodeX {
    RequiresParamToEqualNodeX(const Node& node, int x) {
        REQUIRE(node["x"].to<int>() == x);
    }
    RequiresParamToEqualNodeX(int any) {}
};

TEST_CASE("ConfigNode inheritance") {
    auto yaml = YAML::Load(R"(
a: 7
b: 900
Children:
  a: 100
  One: {}
  Two: {a: 9}
  Three: {b: 70}
  Four:
    FourChild:
      a: 11
)");
    Node node(yaml);

    SECTION("What does YAML::Node do?") {
        REQUIRE(yaml["does"]["not"]["exist"].as<int>(9) == 9);
    }

    SECTION("Parent traversal") {
        REQUIRE(node["a"].to<int>() == 7);
        REQUIRE(node["Children"]["a"].to<int>() == 100);
        REQUIRE(node["Children"][".."]["a"].to<int>() == 7);
    }

    SECTION("value_or") {
        {
            auto c = node["c"];
            REQUIRE(c.value_or(1) == 1);
            REQUIRE(node["a"].value_or(100) == 7);
            REQUIRE(node["Children"]["a"].value_or(42) == 100);
            REQUIRE(node["does"]["not"]["exist"].value_or(90) == 90);
            {
                EmptyStruct ctx;
                // TODO: what should this syntax look like?
                //                REQUIRE(node["foo"].to<MyType>(&ctx).value_or("from_other_ctor").msg
                //                == "from_other_ctor");
            }
        }
    }

    SECTION("Inheritance") {
        {
            int b = node["Children"]["b"].to<int>();
            REQUIRE(b == 900);
        }
        {
            int b = node["Children"]["One"]["b"].to<int>();
            REQUIRE(b == 900);
        }
        {
            int b = node["Children"]["Three"]["b"].to<int>();
            REQUIRE(b == 70);
        }
    }

    SECTION("No inheritance") {
        {
            int a = node["a"].to<int>();
            REQUIRE(a == 7);
        }

        {
            int a = node["Children"]["a"].to<int>();
            REQUIRE(a == 100);
        }

        {
            int b = node["Children"]["Three"]["b"].to<int>();
            REQUIRE(b == 70);
        }
    }
}

TEST_CASE("ConfigNode Built-Ins Construction") {
    auto yaml = YAML::Load(R"(
SomeString: some_string
IntList: [1,2,3]
ListOfMapStringString:
- {a: A}
- {b: B}
)");
    Node node(yaml);

    { REQUIRE(node["SomeString"].to<std::string>() == "some_string"); }
    { REQUIRE((node["IntList"].to<std::vector<int>>() == std::vector<int>{1, 2, 3})); }
    {
        using ListMapStrStr = std::vector<std::map<std::string, std::string>>;
        auto expect = ListMapStrStr{{{"a", "A"}}, {{"b", "B"}}};
        auto actual = node["ListOfMapStringString"].to<ListMapStrStr>();

        REQUIRE(expect == actual);
    }
}


TEST_CASE("ConfigNode Simple User-Defined Conversions") {
    EmptyStruct context;

    auto yaml = YAML::Load(R"(
msg: bar
One: {msg: foo}
Two: {}
)");
    Node node(yaml);

    {
        TakesEmptyStructAndExtractsMsg one =
            node["One"].to<TakesEmptyStructAndExtractsMsg>(&context);
        REQUIRE(one.msg == "foo");
    }
    {
        TakesEmptyStructAndExtractsMsg one =
            node["Two"].to<TakesEmptyStructAndExtractsMsg>(&context);
        REQUIRE(one.msg == "bar");
    }
}

TEST_CASE("maybe") {
    auto yaml = YAML::Load(R"(
Children:
  msg: inherited
  overrides: {msg: overridden}
  deep:
    nesting:
      can:
        still: {inherit: {}, override: {msg: deeply_overridden}}
)");
    Node node(yaml);

    node["does"]["not"]["exist"].maybe<RequiresParamToEqualNodeX>(3);
    REQUIRE(node["Children"].maybe<ExtractsStringMsgOrTakesStringCtor>()->msg == "inherited");
    REQUIRE(node["Children"]["overrides"].maybe<ExtractsStringMsgOrTakesStringCtor>()->msg == "overridden");
    REQUIRE(node["Children"]["deep"]["nesting"]["can"]["still"]["inherit"].maybe<ExtractsStringMsgOrTakesStringCtor>()->msg == "inherited");
    REQUIRE(node["Children"]["deep"]["nesting"]["can"]["still"]["override"].maybe<ExtractsStringMsgOrTakesStringCtor>()->msg == "deeply_overridden");
}

TEST_CASE("Configurable additional-ctor-params Conversions") {
    auto yaml = YAML::Load(R"(
x: 9
a: {x: 7}
b: {}
)");
    Node node(yaml);

    node.to<RequiresParamToEqualNodeX>(9);
    node["a"].to<RequiresParamToEqualNodeX>(7);
    node["b"].to<RequiresParamToEqualNodeX>(9);
}