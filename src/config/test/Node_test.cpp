#include <config/Node.hpp>

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

struct HasConversionSpecialization {
    int x;
};

namespace genny {
template <>
struct NodeConvert<HasConversionSpecialization> {
    using type = HasConversionSpecialization;
    static type convert(const Node& node, int delta) {
        return {node["x"].to<int>() + delta};
    }
};
}  // namespace genny

TEST_CASE("Static Failures") {
    Node node{"{}", ""};

    STATIC_REQUIRE(std::is_same_v<decltype(node.to<int>()), int>);
    STATIC_REQUIRE(std::is_same_v<decltype(node.to<HasConversionSpecialization>()),
                                  HasConversionSpecialization>);
}

TEST_CASE("YAML::Node Equivalency") {

    //
    // The assertions on YAML::Node aren't strictly necessary, but
    // having the API shown here justifies the behavior of genny::Node.
    //
    {
        YAML::Node yaml = YAML::Load("foo: false");
        REQUIRE(yaml);
        REQUIRE(yaml["foo"]);
        REQUIRE(yaml["foo"].IsScalar());
        REQUIRE(yaml["foo"].as<bool>() == false);
    }

    // iteration cases
    {// iteration over sequences
     {YAML::Node yaml = YAML::Load("ns: [1,2,3]");
    int sum = 0;
    for (auto n : yaml["ns"]) {
        REQUIRE(n.first.IsDefined() == false);
        REQUIRE(n.second.IsDefined() == false);
        sum += n.as<int>();
    }
    REQUIRE(sum == 6);
}

{
    Node node{"ns: [1,2,3]", ""};
    int sum = 0;
    for (auto n : node["ns"]) {
        REQUIRE(bool(n.first) == false);
        REQUIRE(bool(n.second) == false);
        sum += n.to<int>();
        if (sum == 1) {
            REQUIRE(n.first.path() == "/ns/0$key");
            REQUIRE(n.second.path() == "/ns/0");
        }
    }
    REQUIRE(sum == 6);
}
}

{// iteration over maps
 {YAML::Node yaml = YAML::Load("foo: bar");
int seen = 0;
for (auto kvp : yaml) {
    ++seen;
    REQUIRE(kvp.first.as<std::string>() == "foo");
    REQUIRE(kvp.second.as<std::string>() == "bar");
}
REQUIRE(seen == 1);
}
{
    Node node = Node("foo: bar", "");
    int seen = 0;
    for (auto kvp : node) {
        ++seen;
        REQUIRE(kvp.first.to<std::string>() == "foo");
        REQUIRE(kvp.second.to<std::string>() == "bar");
        // not super well-defined but what else can we do?
        REQUIRE(kvp.path() == "/0");
        REQUIRE(kvp.first.path() == "/foo$key");
        REQUIRE(kvp.second.path() == "/foo");
    }
    REQUIRE(seen == 1);
}
}

{
    // This is why we need to have Node::_valid
    // We can't rely on our default-constructed nodes.
    auto defaultConstructed = YAML::Node{};
    REQUIRE(bool(defaultConstructed) == true);
    REQUIRE(defaultConstructed.Type() == YAML::NodeType::Null);
}

{
    // we're equivalent to YAML::Node's handling of
    // null and missing values
    YAML::Node yaml = YAML::Load("foo: null");

    REQUIRE(yaml["foo"].IsDefined() == true);
    REQUIRE(yaml["foo"].IsNull() == true);
    REQUIRE(bool(yaml["foo"]) == true);

    REQUIRE(yaml["bar"].IsDefined() == false);
    REQUIRE(yaml["bar"].IsNull() == false);
    REQUIRE(bool(yaml["bar"]) == false);

    Node node{"foo: null", ""};
    REQUIRE(node["foo"].isNull() == true);
    REQUIRE(bool(node["foo"]) == true);

    REQUIRE(node["bar"].isNull() == false);
    REQUIRE(bool(node["bar"]) == false);
}

{
    YAML::Node yaml = YAML::Load("{a: A, b: B}");
    REQUIRE(yaml.as<std::map<std::string, std::string>>() ==
            std::map<std::string, std::string>{{"a", "A"}, {"b", "B"}});
}

{
    YAML::Node yaml = YAML::Load("a: null");
    REQUIRE(yaml["a"].IsNull());
    REQUIRE(yaml["a"].as<int>(7) == 7);
}
}

TEST_CASE("value_or") {
    auto yaml = std::string(R"(
seven: 7
bee: b
intList: [1,2,3]
stringMap: {a: A, b: B}
nothing: null
sure: true
nope: false
)");
    Node node{yaml, ""};
    REQUIRE(node["seven"].value_or(8) == 7);
    REQUIRE(node["eight"].value_or(8) == 8);
    REQUIRE(node["intList"].value_or(std::vector<int>{}) == std::vector<int>{1, 2, 3});
    REQUIRE(node["intList2"].value_or(std::vector<int>{1, 2}) == std::vector<int>{1, 2});
    REQUIRE(node["stringMap"].value_or(std::map<std::string, std::string>{}) ==
            std::map<std::string, std::string>{{"a", "A"}, {"b", "B"}});
    REQUIRE(node["stringMap2"].value_or(std::map<std::string, std::string>{{"foo", "bar"}}) ==
            std::map<std::string, std::string>{{"foo", "bar"}});
    REQUIRE(node["nothing"].value_or(7) == 7);

    REQUIRE(node["sure"].value_or(false) == true);
    REQUIRE(node["sure"].value_or(true) == true);
    REQUIRE(node["nope"].value_or(false) == false);
    REQUIRE(node["nope"].value_or(true) == false);
    REQUIRE(node["doesntExist"].value_or(true) == true);
    REQUIRE(node["doesntExist"].value_or(false) == false);

    REQUIRE(node["bee"].value_or<std::string>("foo") == "b");
    REQUIRE(node["baz"].value_or<std::string>("foo") == "foo");


    REQUIRE(node["stringMap"]["a"].value_or<std::string>("7") == "A");
    // inherits from parent
    REQUIRE(node["stringMap"]["bee"].value_or<std::string>("7") == "b");
}

TEST_CASE("Node Type") {
    auto yaml = std::string(R"(
seven: 7
bee: b
mixedList: [1,2,"a", [inner]]
mixedMap: {seven: 7, bees: [b]}
nothing: null
sure: true
nope: false
)");
    Node node{yaml, ""};
    REQUIRE(node["nonexistant"].type() == Node::NodeType::Undefined);

    REQUIRE(node.type() == Node::NodeType::Map);
    REQUIRE(node.isMap());

    REQUIRE(node["seven"].isScalar());
    REQUIRE(node["seven"].type() == Node::NodeType::Scalar);

    REQUIRE(node["bee"].isScalar());
    REQUIRE(node["bee"].type() == Node::NodeType::Scalar);

    REQUIRE(node["mixedList"].isSequence());
    REQUIRE(node["mixedList"].type() == Node::NodeType::Sequence);

    REQUIRE(node["mixedList"][0].isScalar());
    REQUIRE(node["mixedList"][0].type() == Node::NodeType::Scalar);

    REQUIRE(node["mixedList"][3].isSequence());
    REQUIRE(node["mixedList"][3].type() == Node::NodeType::Sequence);

    REQUIRE(node["mixedMap"].isMap());
    REQUIRE(node["mixedMap"].type() == Node::NodeType::Map);

    REQUIRE(node["mixedMap"]["seven"].isScalar());
    REQUIRE(node["mixedMap"]["seven"].type() == Node::NodeType::Scalar);

    REQUIRE(node["mixedMap"]["bees"].isSequence());
    REQUIRE(node["mixedMap"]["bees"].type() == Node::NodeType::Sequence);

    REQUIRE(node["nothing"].isNull());
    REQUIRE(node["nothing"].type() == Node::NodeType::Null);

    REQUIRE(node["sure"].isScalar());
    REQUIRE(node["sure"]);
    REQUIRE(!!node["sure"]);
    REQUIRE(node["sure"].to<bool>());

    auto sure = node["sure"].maybe<bool>();
    REQUIRE(sure);
    REQUIRE(*sure);
    REQUIRE(node["sure"].to<bool>());

    REQUIRE(node["nope"].isScalar());
    auto foo = node["nope"].maybe<bool>();
    REQUIRE(foo);
    REQUIRE(*foo == false);
    REQUIRE(node["nope"].to<bool>() == false);
}

TEST_CASE("size") {
    {
        Node node{"foo: bar", ""};
        REQUIRE(node.size() == 1);
        // scalars have size 0
        REQUIRE(node["foo"].size() == 0);
    }
    {
        Node node{"{}", ""};
        REQUIRE(node.size() == 0);
    }
    {
        Node node{"a: null", ""};
        REQUIRE(node["a"].size() == 0);
    }
    {
        Node node{"[1,2,3]", ""};
        REQUIRE(node.size() == 3);
    }
    {
        Node node{"a: {b: {c: []}}", ""};
        REQUIRE(node.size() == 1);
        REQUIRE(node["a"].size() == 1);
        REQUIRE(node["a"]["b"].size() == 1);
        REQUIRE(node["a"]["b"]["c"].size() == 0);
    }
    {
        Node node{"", ""};
        REQUIRE(node.size() == 0);
    }
    {
        Node node{
            "foos: [1,2,3]\n"
            "children: {a: 7}",
            ""};
        REQUIRE(node.size() == 2);
        REQUIRE(node["foos"].size() == 3);
        // inheritance
        REQUIRE(node["children"]["foos"].size() == 3);
        REQUIRE(node["children"].size() == 1);
        // scalars have size 0
        REQUIRE(node["children"]["a"].size() == 0);
    }
    {
        Node node{
            "foos: [1,2,3]\n"
            "children: {a: 7}",
            ""};
        REQUIRE(node["foos"][".."].size() == 2);
        REQUIRE(node["foos"][".."][".."].size() == 0);
        REQUIRE(node["foos"][".."][".."][".."].size() == 0);
        REQUIRE(node["foos"][".."][".."][".."][".."].size() == 0);
    }
}

TEST_CASE("Node inheritance") {
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

TEST_CASE("Node Built-Ins Construction") {
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

TEST_CASE("Specialization") {
    Node node("{x: 8}", "");
    REQUIRE(node.to<HasConversionSpecialization>(3).x == 11);
}

TEST_CASE("Node Paths") {
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
        Catch::Matches(
            R"(Invalid key 'foo': Tried to access node that doesn't exist. On node with path '/One/foo': )"));
}

TEST_CASE("Node iteration path") {
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

TEST_CASE("Node Simple User-Defined Conversions") {
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

TEST_CASE("operator-left-shift") {
    {
        Node node{"Foo: 7", ""};
        std::stringstream str;
        str << node;
        REQUIRE(str.str() == "Foo: 7");
    }
    {
        Node node{"Foo: {Bar: Baz}", ""};
        std::stringstream str;
        str << node;
        REQUIRE(str.str() == "Foo: {Bar: Baz}");
    }
    // rely on YAML::Dump so don't need to enforce much beyond this
}

TEST_CASE("getPlural") {
    {
        Node node{"Foo: 7", ""};
        REQUIRE(node.getPlural<int>("Foo", "Foos") == std::vector<int>{7});
    }
    {
        Node node{"Foos: [1,2,3]", ""};
        REQUIRE(node.getPlural<int>("Foo", "Foos") == std::vector<int>{1, 2, 3});
    }
    {
        Node node{"Foo: 712", ""};
        int calls = 0;
        REQUIRE(node.getPlural<HasConversionSpecialization>("Foo",
                                                            "Foos",
                                                            [&](const Node& node) {
                                                                ++calls;
                                                                // add one to the node value
                                                                return HasConversionSpecialization{
                                                                    node.to<int>() + 1};
                                                            })[0]
                    .x == 713);
        REQUIRE(calls == 1);
    }

    {
        Node node{"Foos: [1,2,3]", ""};
        int calls = 0;
        REQUIRE(node.getPlural<HasConversionSpecialization>("Foo",
                                                            "Foos",
                                                            [&](const Node& node) {
                                                                ++calls;
                                                                // subtract 1 from the node value
                                                                return HasConversionSpecialization{
                                                                    node.to<int>() - 1};
                                                            })[2]
                    .x == 2);
        REQUIRE(calls == 3);
    }

    {
        Node node{"{}", ""};
        REQUIRE_THROWS_WITH(
            [&]() { node.getPlural<int>("Foo", "Foos"); }(),
            Catch::Matches(
                R"(Invalid key '\$plural\(Foo,Foos\)': Either 'Foo' or 'Foos' required. On node with path '': \{\})"));
    }

    {
        Node node{"{Foos: 7}", ""};
        REQUIRE_THROWS_WITH(
            [&]() { node.getPlural<int>("Foo", "Foos"); }(),
            Catch::Matches(
                R"(Invalid key '\$plural\(Foo,Foos\)': Plural 'Foos' must be a sequence type. On node with path '': \{Foos: 7\})"));
    }

    {
        Node node{"{Foo: 8, Foos: [1,2]}", ""};
        REQUIRE_THROWS_WITH(
            [&]() { node.getPlural<int>("Foo", "Foos"); }(),
            Catch::Matches(
                R"(Invalid key '\$plural\(Foo,Foos\)': Can't have both 'Foo' and 'Foos'. On node with path '': \{Foo: 8, Foos: \[1, 2\]\})"));
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
