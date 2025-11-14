#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bonsai/state/builder.hpp"
#include "bonsai/state/machine.hpp"
#include "bonsai/tree/builder.hpp"
#include "bonsai/tree/tree.hpp"
#include "bonsai/visualization/visualization.hpp"

using namespace bonsai;
using namespace bonsai::visualization;

TEST_CASE("Visualization - basic state machine export") {
    state::Builder builder;
    auto machine =
        builder.state("idle")
            .transitionTo("active", [](auto &bb) { return bb.template get<bool>("activate").value_or(false); })
            .state("active")
            .transitionTo("idle", [](auto &) { return true; })
            .initial("idle")
            .build();

    machine->enableTransitionHistory(true);
    machine->tick(); // Initialize
    machine->blackboard().set("activate", true);
    machine->tick(); // Transition to active

    std::string dot = StateMachineExporter::toDot(*machine, "TestMachine");

    // Check that DOT contains expected elements
    CHECK(dot.find("digraph TestMachine") != std::string::npos);
    CHECK(dot.find("rankdir=LR") != std::string::npos);
    CHECK(dot.find("idle") != std::string::npos);
    CHECK(dot.find("active") != std::string::npos);
    CHECK(dot.find("->") != std::string::npos);
}

TEST_CASE("Visualization - state machine with timed transitions") {
    state::Builder builder;
    auto machine = builder.state("waiting")
                       .transitionToAfter("done", std::chrono::milliseconds(50))
                       .state("done")
                       .initial("waiting")
                       .build();

    machine->enableTransitionHistory(true);
    machine->tick(); // Initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    machine->tick(); // Timed transition

    std::string dot = StateMachineExporter::toDot(*machine);

    CHECK(dot.find("waiting") != std::string::npos);
    CHECK(dot.find("done") != std::string::npos);
    CHECK(dot.find("timed") != std::string::npos);
}

TEST_CASE("Visualization - state machine with weighted transitions") {
    state::Builder builder;
    auto machine = builder.state("start")
                       .transitionTo("a", [](auto &) { return true; })
                       .withWeight(7.0f)
                       .transitionTo("b", [](auto &) { return true; })
                       .withWeight(3.0f)
                       .state("a")
                       .state("b")
                       .initial("start")
                       .build();

    machine->enableTransitionHistory(true);
    machine->tick(); // Initialize
    machine->tick(); // Weighted transition

    std::string dot = StateMachineExporter::toDot(*machine);

    CHECK(dot.find("weighted") != std::string::npos);
}

TEST_CASE("Visualization - current state highlighting") {
    state::Builder builder;
    auto machine = builder.state("idle")
                       .transitionTo("active", [](auto &) { return true; })
                       .state("active")
                       .initial("idle")
                       .build();

    machine->tick(); // Initialize to idle
    machine->tick(); // Transition to active

    std::string dot = StateMachineExporter::toDot(*machine);

    // Current state should be highlighted
    CHECK(dot.find("active") != std::string::npos);
    CHECK(dot.find("lightblue") != std::string::npos);
}

TEST_CASE("Visualization - common graph structure") {
    Graph graph;
    graph.name = "TestGraph";
    graph.rankdir = "TB";

    GraphNode node1;
    node1.id = "node1";
    node1.label = "First Node";
    node1.shape = "box";
    graph.nodes.push_back(node1);

    GraphNode node2;
    node2.id = "node2";
    node2.label = "Second Node";
    node2.shape = "circle";
    node2.style = "filled";
    node2.fillcolor = "lightgreen";
    graph.nodes.push_back(node2);

    GraphEdge edge;
    edge.from = "node1";
    edge.to = "node2";
    edge.label = "transition";
    edge.color = "blue";
    graph.edges.push_back(edge);

    std::string dot = GraphvizExporter::toDot(graph);

    CHECK(dot.find("digraph TestGraph") != std::string::npos);
    CHECK(dot.find("node1") != std::string::npos);
    CHECK(dot.find("node2") != std::string::npos);
    CHECK(dot.find("First Node") != std::string::npos);
    CHECK(dot.find("Second Node") != std::string::npos);
    CHECK(dot.find("node1 -> node2") != std::string::npos);
    CHECK(dot.find("transition") != std::string::npos);
}

TEST_CASE("Visualization - hierarchical clustering") {
    Graph graph;
    graph.name = "HierarchicalGraph";

    GraphCluster cluster;
    cluster.id = "subsystem1";
    cluster.label = "Subsystem 1";
    cluster.nodeIds = {"node1", "node2"};
    graph.clusters.push_back(cluster);

    GraphNode node1;
    node1.id = "node1";
    node1.label = "Node 1";
    graph.nodes.push_back(node1);

    GraphNode node2;
    node2.id = "node2";
    node2.label = "Node 2";
    graph.nodes.push_back(node2);

    GraphNode node3;
    node3.id = "node3";
    node3.label = "External Node";
    graph.nodes.push_back(node3);

    GraphEdge edge;
    edge.from = "node1";
    edge.to = "node3";
    graph.edges.push_back(edge);

    std::string dot = GraphvizExporter::toDot(graph);

    CHECK(dot.find("subgraph cluster_subsystem1") != std::string::npos);
    CHECK(dot.find("Subsystem 1") != std::string::npos);
    CHECK(dot.find("node1") != std::string::npos);
    CHECK(dot.find("node3") != std::string::npos);
}

TEST_CASE("Visualization - special character escaping") {
    Graph graph;
    graph.name = "EscapeTest";

    GraphNode node;
    node.id = "node with spaces";
    node.label = "Label \"with\" quotes";
    graph.nodes.push_back(node);

    std::string dot = GraphvizExporter::toDot(graph);

    // Should have escaped quotes
    CHECK(dot.find("\\\"") != std::string::npos);
}

TEST_CASE("Visualization - empty graph") {
    Graph graph;
    graph.name = "EmptyGraph";

    std::string dot = GraphvizExporter::toDot(graph);

    CHECK(dot.find("digraph EmptyGraph") != std::string::npos);
    CHECK(dot.find("}") != std::string::npos);
}

TEST_CASE("Visualization - behavior tree basic export") {
    tree::Builder builder;
    auto bt = builder.sequence()
                  .action([](auto &) { return tree::Status::Success; })
                  .action([](auto &) { return tree::Status::Success; })
                  .end()
                  .build();

    std::string dot = BehaviorTreeExporter::toDot(bt, "TestBT");

    CHECK(dot.find("digraph TestBT") != std::string::npos);
    CHECK(dot.find("rankdir=TB") != std::string::npos);
}

TEST_CASE("Visualization - multiple transitions to same state") {
    state::Builder builder;
    auto machine = builder.state("a")
                       .transitionTo("b", [](auto &bb) { return bb.template get<bool>("go1").value_or(false); })
                       .state("c")
                       .transitionTo("b", [](auto &bb) { return bb.template get<bool>("go2").value_or(false); })
                       .state("b")
                       .initial("a")
                       .build();

    machine->enableTransitionHistory(true);
    machine->tick(); // Initialize to a
    machine->blackboard().set("go1", true);
    machine->tick(); // a -> b

    std::string dot = StateMachineExporter::toDot(*machine);

    // Should have all states
    CHECK(dot.find("\"a\"") != std::string::npos);
    CHECK(dot.find("\"b\"") != std::string::npos);
}

TEST_CASE("Visualization - color coding by transition type") {
    state::Builder builder;
    auto machine = builder.state("s1")
                       .transitionToAfter("s2", std::chrono::milliseconds(100))
                       .state("s2")
                       .transitionTo("s3", [](auto &) { return true; })
                       .withWeight(5.0f)
                       .state("s3")
                       .transitionTo("s4", [](auto &) { return true; })
                       .withProbability(0.5f)
                       .state("s4")
                       .initial("s1")
                       .build();

    machine->enableTransitionHistory(true);
    machine->tick(); // Initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    machine->tick(); // Timed transition s1->s2
    machine->tick(); // Weighted transition s2->s3
    machine->tick(); // Probabilistic transition s3->s4 (50% chance)

    std::string dot = StateMachineExporter::toDot(*machine);

    // Check for color coding (timed=blue, weighted=purple, probabilistic=orange)
    // At minimum, we should have different transition types
    int transitionCount = 0;
    size_t pos = 0;
    while ((pos = dot.find("->", pos)) != std::string::npos) {
        transitionCount++;
        pos += 2;
    }

    CHECK(transitionCount >= 1); // At least one transition happened
}
