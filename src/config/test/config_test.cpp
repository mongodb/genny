#include <config/config.hpp>

#include <catch2/catch.hpp>

#include <map>
#include <vector>

using namespace config;

struct Ctx {
    int rng() const { return 7; }
};

struct MyType {
    std::string msg;
    MyType(std::string msg) : msg{msg} {}
    MyType(const Node& node, Ctx* ctx) : msg{node["msg"].to<std::string>()} {
        REQUIRE(ctx->rng() == 7);
    }
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
                Ctx ctx;
                // TODO: what should this syntax look like?
//                REQUIRE(node["foo"].to<MyType>(&ctx).value_or("from_other_ctor").msg == "from_other_ctor");
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
    Ctx context;

    auto yaml = YAML::Load(R"(
msg: bar
One: {msg: foo}
Two: {}
)");
    Node node(yaml);

    {
        MyType one = node["One"].to<MyType>(&context);
        REQUIRE(one.msg == "foo");
    }
    {
        MyType one = node["Two"].to<MyType>(&context);
        REQUIRE(one.msg == "bar");
    }
}

struct HasOtherCtorParams {
    int x;
    HasOtherCtorParams(const Node& node, Ctx* ctx, int x) : x{x} {
        REQUIRE(node["x"].to<int>() == x);
    }
};

TEST_CASE("Configurable additional-ctor-params Conversions") {
    Ctx context;

    auto yaml = YAML::Load(R"(
x: 9
a: {x: 7}
b: {}
)");
    Node node(yaml);
    {
        HasOtherCtorParams one = node.to<HasOtherCtorParams>(&context, 9);
        REQUIRE(one.x == 9);
    }
    {
        HasOtherCtorParams two = node["a"].to<HasOtherCtorParams>(&context, 7);
        REQUIRE(two.x == 7);
    }
    {
        HasOtherCtorParams three = node["b"].to<HasOtherCtorParams>(&context, 9);
        REQUIRE(three.x == 9);
    }
}