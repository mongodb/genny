#include "catch.hpp"
#include "document.hpp"
#include "workload.hpp"
#include "sleep.hpp"
#include "doAll.hpp"
#include "finish_node.hpp"
#include "random_choice.hpp"
#include "workloadNode.hpp"

using namespace mwg;

TEST_CASE("Generate Dot Files", "[dot]") {
    SECTION("Default node behavior") {
        auto yaml = YAML::Load(R"yaml(
    type : sleep
    name : sleep
    next : nextNode
    sleep : 1000)yaml");

        auto node = sleepNode(yaml);
        string expected = "sleep -> nextNode;\n";
        string expectedExtra;
        auto graph = node.generateDotGraph();
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << graph.first << "\"");
        REQUIRE(expected.compare(graph.first) == 0);
        INFO("Expected = \"" << expectedExtra << "\"");
        INFO("Got      = \"" << graph.second << "\"");
        REQUIRE(expectedExtra.compare(graph.second) == 0);
    }

    SECTION("DoAll node behavior") {
        auto yaml = YAML::Load(R"yaml(
      name : doAll
      type : doAll
      childNodes : 
        - thingA
        - thingB
      next : join)yaml");

        auto node = doAll(yaml);
        string expected = "doAll -> thingA;\ndoAll -> thingB;\ndoAll -> join;\n";
        string expectedExtra;
        auto graph = node.generateDotGraph();
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << graph.first << "\"");
        REQUIRE(expected.compare(graph.first) == 0);
        INFO("Expected = \"" << expectedExtra << "\"");
        INFO("Got      = \"" << graph.second << "\"");
        REQUIRE(expectedExtra.compare(graph.second) == 0);
    }
    SECTION("Finish node") {
        auto node = finishNode();
        string expected = "";
        auto graph = node.generateDotGraph();
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << graph.first << "\"");
        REQUIRE(expected.compare(graph.first) == 0);
        INFO("Got      = \"" << graph.second << "\"");
        REQUIRE(expected.compare(graph.second) == 0);
    }
    SECTION("Random choice node behavior") {
        auto yaml = YAML::Load(R"yaml(
      name : random
      print : In Random Choice
      type : random_choice
      next : 
        insert2 : 0.5
        query : 0.5)yaml");

        auto node = random_choice(yaml);
        string expected =
            "random -> insert2[label=\"0.500000\"];\nrandom -> query[label=\"0.500000\"];\n";
        string expectedExtra;
        auto graph = node.generateDotGraph();
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << graph.first << "\"");
        REQUIRE(expected.compare(graph.first) == 0);
        INFO("Expected = \"" << expectedExtra << "\"");
        INFO("Got      = \"" << graph.second << "\"");
        REQUIRE(expectedExtra.compare(graph.second) == 0);
    }
    SECTION("Workload graph") {
        auto yaml = YAML::Load(R"yaml(
        name : main 
        nodes : 
          - name : sleep
            type : sleep
            sleep : 1
            next : Finish
            print : In sleep)yaml");

        workload myworkload(yaml);
        string expected = "digraph main {\nsleep -> Finish;\n}\n";
        auto graph = myworkload.generateDotGraph();
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << graph << "\"");
        REQUIRE(expected.compare(graph) == 0);
    }
    SECTION("Workload node behavior") {
        auto yaml = YAML::Load(R"yaml(
      name : workload
      type : workloadNode
      next : Finish
      workload :
        name : embeddedWorkload
        nodes :
          - name : sleep
            type : sleep
            sleep : 1
            next : Finish
            print : In sleep)yaml");
        auto node = workloadNode(yaml);
        string expected = "workload -> Finish;\n";
        string expectedExtra = "digraph embeddedWorkload {\nsleep -> Finish;\n}\n";
        auto graph = node.generateDotGraph();
        INFO("Expected = \"" << expected << "\"");
        INFO("Got      = \"" << graph.first << "\"");
        REQUIRE(expected.compare(graph.first) == 0);
        INFO("Expected = \"" << expectedExtra << "\"");
        INFO("Got      = \"" << graph.second << "\"");
        REQUIRE(expectedExtra.compare(graph.second) == 0);
    }
}
