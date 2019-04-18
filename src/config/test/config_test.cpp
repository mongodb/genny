#include <config/config.hpp>
#include <testlib/helpers.hpp>
#include <catch2/catch.hpp>

#include <vector>
#include <map>

using namespace genny;

struct Ctx {
    int rng() { return 7; }
};

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
    Node<Ctx> node(yaml, &context);

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
    Node<Ctx> node(yaml, &context);

    {
        REQUIRE(node["SomeString"].as<std::string>() == "some_string");
    }
    {
        REQUIRE((node["IntList"].as<std::vector<int>>() == std::vector<int>{1,2,3}));
    }
    {
        using ListMapStrStr = std::vector<std::map<std::string,std::string>>;
        auto expect = ListMapStrStr{
                {{"a", "A"}}, {{"b", "B"}}
        };
        auto actual = node["ListOfMapStringString"].as<ListMapStrStr>();

        REQUIRE(expect == actual);
    }
}


namespace genny {

struct MyType {
    std::string msg;
    MyType(Node<Ctx>& node, Ctx* ctx)
    : msg{node["msg"].as<std::string>()}{
        REQUIRE(ctx->rng() == 7);
    }
};

}

TEST_CASE("ConfigNode User-Defined Conversions") {
    Ctx context;

    auto yaml = YAML::Load(R"(
msg: bar
One: {msg: foo}
Two: {}
)");
    Node node(yaml, &context);

    {
        MyType one = node["One"].as<MyType>();
        REQUIRE(one.msg == "foo");
    }
    {
        MyType one = node["Two"].as<MyType>();
        REQUIRE(one.msg == "bar");
    }
}

