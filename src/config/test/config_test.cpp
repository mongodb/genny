#include <config/config.hpp>

#include <catch2/catch.hpp>

#include <map>
#include <vector>

using namespace genny;

struct EmptyStruct {};

struct ExtractsMsg {
    std::string msg;
    ExtractsMsg(const Node& node) : msg{node["msg"].to<std::string>()} {}
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
    auto yaml = std::string(R"(
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
    // TODO: when using this in the Driver, use the file name or "<root>" or something more obvious
    Node node(yaml, "");

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
    auto yaml = std::string(R"(
SomeString: some_string
IntList: [1,2,3]
ListOfMapStringString:
- {a: A}
- {b: B}
)");
    Node node(yaml, "");

    { REQUIRE(node["SomeString"].to<std::string>() == "some_string"); }
    { REQUIRE((node["IntList"].to<std::vector<int>>() == std::vector<int>{1, 2, 3})); }
    {
        using ListMapStrStr = std::vector<std::map<std::string, std::string>>;
        auto expect = ListMapStrStr{{{"a", "A"}}, {{"b", "B"}}};
        auto actual = node["ListOfMapStringString"].to<ListMapStrStr>();

        REQUIRE(expect == actual);
    }
}

TEST_CASE("ConfigNode Paths") {
    auto yaml = std::string(R"(
msg: bar
One: {msg: foo}
Two: {}
)");
    Node node(yaml, "");
    REQUIRE(node["One"][".."].path() == "/One/..");
    REQUIRE(node.path() == "");
    REQUIRE(node[0].path() == "/0");
    REQUIRE(node["msg"].path() == "/msg");
    REQUIRE(node["msg"][".."].path() == "/msg/..");
    REQUIRE(node["msg"][".."][".."][".."][".."].path() == "/msg/../../../..");
    REQUIRE(node["One"]["msg"].path() == "/One/msg");
    REQUIRE(node["One"]["msg"][".."].path() == "/One/msg/..");
    REQUIRE(node["One"]["msg"][".."]["msg"][".."]["msg"].path() == "/One/msg/../msg/../msg");
    REQUIRE(node["One"]["foo"][0][1]["bar"].path() == "/One/foo/0/1/bar");
    REQUIRE(node["One"]["foo"][0][1]["bar"][".."].path() == "/One/foo/0/1/bar/..");

    REQUIRE_THROWS_WITH(
        node["One"]["foo"].to<std::string>(),
        Catch::Contains("Tried to access node that doesn't exist at path: /One/foo"));
}

TEST_CASE("ConfigNode iteration path") {
    auto yaml = std::string(R"(
one: [1]
two: [1,2]
mapOneDeep: {a: A}
mapTwoDeep: {a: {A: aA}}
)");
    Node node(yaml, "");
    {
        int seen = 0;
        for (auto&& n : node["one"]) {
            REQUIRE(n.path() == "/one/0");
            ++seen;
        }
        REQUIRE(seen == 1);
    }
    {
        int seen = 0;
        for (auto&& n : node["two"]) {
            REQUIRE(n.path() == "/two/" + std::to_string(seen));
            ++seen;
        }
        REQUIRE(seen == 2);
    }
    {
        int seen = 0;
        for (auto&& kvp : node["mapOneDeep"]) {
            // this isn't super well-defined - what's the "path" for the key of a kvp?
            REQUIRE(kvp.first.path() == "/mapOneDeep/a$key");
            REQUIRE(kvp.first[".."].path() == "/mapOneDeep/a$key/..");
            REQUIRE(kvp.second.path() == "/mapOneDeep/a");
            REQUIRE(kvp.second[".."].path() == "/mapOneDeep/a/..");
            ++seen;
        }
        REQUIRE(seen == 1);
    }
}

TEST_CASE("ConfigNode Simple User-Defined Conversions") {
    EmptyStruct context;

    auto yaml = std::string(R"(
msg: bar
One: {msg: foo}
Two: {}
)");
    Node node(yaml, "");

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
    auto yaml = std::string(R"(
Children:
  msg: inherited
  overrides: {msg: overridden}
  deep:
    nesting:
      can:
        still: {inherit: {}, override: {msg: deeply_overridden}}
)");
    Node node(yaml, "");

    node["does"]["not"]["exist"].maybe<RequiresParamToEqualNodeX>(3);
    REQUIRE(!node["does"]["not"]["exist"].maybe<ExtractsMsg>());
    REQUIRE(node["Children"].maybe<ExtractsMsg>()->msg == "inherited");
    REQUIRE(node["Children"]["overrides"].maybe<ExtractsMsg>()->msg == "overridden");
    REQUIRE(
        node["Children"]["deep"]["nesting"]["can"]["still"]["inherit"].maybe<ExtractsMsg>()->msg ==
        "inherited");
    REQUIRE(
        node["Children"]["deep"]["nesting"]["can"]["still"]["override"].maybe<ExtractsMsg>()->msg ==
        "deeply_overridden");
}

TEST_CASE("Configurable additional-ctor-params Conversions") {
    auto yaml = std::string(R"(
x: 9
a: {x: 7}
b: {}
)");
    Node node(yaml, "");

    node.to<RequiresParamToEqualNodeX>(9);
    node["a"].to<RequiresParamToEqualNodeX>(7);
    node["b"].to<RequiresParamToEqualNodeX>(9);
}

// TODO: how to handle iterating over inherited keys?
TEST_CASE("Iteration") {
    auto yaml = std::string(R"(
Scalar: foo
SimpleMap: {a: b}
ListOfScalars: [1,2]
ListOfMap:
- {a: A, b: B}
SingleItemList: [37]
)");
    Node node(yaml, "");

    SECTION("Scalar") {
        auto a = node["Scalar"];
        REQUIRE(a);
        for (auto kvp : a) {
            FAIL("nothing to iterate");
        }
    }
    SECTION("SimpleMap") {
        auto mp = node["SimpleMap"];
        REQUIRE(mp);
        int seen = 0;
        for (auto kvp : mp) {
            Node k = kvp.first;
            Node v = kvp.second;
            REQUIRE(k.to<std::string>() == "a");
            REQUIRE(v.to<std::string>() == "b");
            ++seen;
        }
        REQUIRE(seen == 1);
    }

    SECTION("ListOfScalars") {
        auto lst = node["ListOfScalars"];
        REQUIRE(lst);
        int i = 1;
        for (auto v : lst) {
            REQUIRE(v.to<int>() == i);
            ++i;
        }
        REQUIRE(i == 3);
    }

    SECTION("ListOfMap") {
        auto lom = node["ListOfMap"];
        REQUIRE(lom);
        REQUIRE(lom.size() == 1);
        auto countMaps = 0;
        for (auto m : lom) {
            ++countMaps;
            REQUIRE(m.size() == 2);

            auto countEntries = 0;
            for (auto kvp : m) {
                ++countEntries;
            }
            REQUIRE(countEntries == 2);

            REQUIRE(m["a"].to<std::string>() == "A");
            REQUIRE(m["b"].to<std::string>() == "B");

            // still get inheritance
            REQUIRE(m["Scalar"].to<std::string>() == "foo");
            // still get parent relationship:s
            REQUIRE(m[".."]["Scalar"].to<std::string>() == "foo");
            REQUIRE(m[".."]["SimpleMap"]["a"][".."]["Scalar"].to<std::string>() == "foo");
        }
        REQUIRE(countMaps == 1);
    }

    SECTION("SingleItemList") {
        auto sil = node["SingleItemList"];
        REQUIRE(sil.size() == 1);
        REQUIRE(sil[0].to<int>() == 37);
        auto count = 0;
        for (auto v : sil) {
            REQUIRE(v.to<int>() == 37);
            // we still get parents
            REQUIRE(v[".."]["Scalar"].to<std::string>() == "foo");
            ++count;
        }
        REQUIRE(count == 1);
    }
}
