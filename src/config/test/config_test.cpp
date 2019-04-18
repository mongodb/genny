#include <catch2/catch.hpp>
#include <config/config.hpp>
#include <testlib/helpers.hpp>

#include <map>
#include <vector>

using namespace genny;

struct Ctx {
    int rng() {
        return 7;
    }
};

using Node = NodeT<Ctx>;

TEST_CASE("ConfigNode inheritance") {
    Ctx context;

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

    SECTION("Parent traversal") {
        REQUIRE(node["a"].as<int>() == 7);
        REQUIRE(node["Children"]["a"].as<int>() == 100);
        REQUIRE(node["Children"][".."]["a"].as<int>() == 7);
    }

    SECTION("Inheritance") {
        {
            int b = node["Children"]["b"].as<int>();
            REQUIRE(b == 900);
        }
        {
            int b = node["Children"]["One"]["b"].as<int>();
            REQUIRE(b == 900);
        }
        {
            int b = node["Children"]["Three"]["b"].as<int>();
            REQUIRE(b == 70);
        }
    }

    SECTION("No inheritance") {
        {
            int a = node["a"].as<int>();
            REQUIRE(a == 7);
        }

        {
            int a = node["Children"]["a"].as<int>();
            REQUIRE(a == 100);
        }

        {
            int b = node["Children"]["Three"]["b"].as<int>();
            REQUIRE(b == 70);
        }
    }
}

TEST_CASE("ConfigNode Built-Ins Construction") {
    Ctx context;

    auto yaml = YAML::Load(R"(
SomeString: some_string
IntList: [1,2,3]
ListOfMapStringString:
- {a: A}
- {b: B}
)");
    Node node(yaml);

    { REQUIRE(node["SomeString"].as<std::string>() == "some_string"); }
    { REQUIRE((node["IntList"].as<std::vector<int>>() == std::vector<int>{1, 2, 3})); }
    {
        using ListMapStrStr = std::vector<std::map<std::string, std::string>>;
        auto expect = ListMapStrStr{{{"a", "A"}}, {{"b", "B"}}};
        auto actual = node["ListOfMapStringString"].as<ListMapStrStr>();

        REQUIRE(expect == actual);
    }
}


struct MyType {
    std::string msg;
    MyType(Node& node, Ctx* ctx) : msg{node["msg"].as<std::string>()} {
        REQUIRE(ctx->rng() == 7);
    }
};

TEST_CASE("ConfigNode Simple User-Defined Conversions") {
    Ctx context;

    auto yaml = YAML::Load(R"(
msg: bar
One: {msg: foo}
Two: {}
)");
    Node node(yaml);

    {
        MyType one = node["One"].from<MyType>(&context);
        REQUIRE(one.msg == "foo");
    }
    {
        MyType one = node["Two"].from<MyType>(&context);
        REQUIRE(one.msg == "bar");
    }
}

struct HasOtherCtorParams {
    int x;
    HasOtherCtorParams(Node& node, Ctx* ctx, int x) : x{x} {
        REQUIRE(node["x"].as<int>() == x);
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
        HasOtherCtorParams one = node.from<HasOtherCtorParams>(&context, 9);
        REQUIRE(one.x == 9);
    }
    {
        HasOtherCtorParams two = node["a"].from<HasOtherCtorParams>(&context, 7);
        REQUIRE(two.x == 7);
    }
    {
        HasOtherCtorParams three = node["b"].from<HasOtherCtorParams>(&context, 9);
        REQUIRE(three.x == 9);
    }
}