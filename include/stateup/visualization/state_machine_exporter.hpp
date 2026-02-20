#pragma once
#include "../state/machine.hpp"
#include "../state/structure/composite_state.hpp"
#include "../state/structure/state.hpp"
#include "../state/structure/transition.hpp"
#include "graphviz.hpp"
#include <memory>

namespace stateup::visualization {

    class StateMachineExporter {
      public:
        // Export state machine to Graphviz DOT format
        static std::string toDot(const state::StateMachine &machine, const std::string &graphName = "StateMachine") {
            Graph graph;
            graph.name = graphName;
            graph.rankdir = "LR";
            graph.attributes["node"] = "[shape=circle]";

            // Add initial state marker
            GraphNode startNode;
            startNode.id = "start";
            startNode.label = "";
            startNode.shape = "point";
            graph.nodes.push_back(startNode);

            // Collect unique states from history and current state
            std::unordered_set<std::string> stateNames;
            stateNames.insert(machine.getCurrentStateName());

            for (const auto &stateName : machine.getStateHistory()) {
                stateNames.insert(stateName);
            }

            // Add state nodes
            for (const auto &stateName : stateNames) {
                if (stateName.empty())
                    continue;

                GraphNode node;
                node.id = stateName;
                node.label = stateName;
                node.shape = "circle";

                // Highlight current state
                if (stateName == machine.getCurrentStateName()) {
                    node.style = "filled";
                    node.fillcolor = "lightblue";
                }

                graph.nodes.push_back(node);
            }

            // Add transitions from history
            if (!machine.getTransitionHistory().empty()) {
                std::unordered_set<std::string> addedTransitions;

                for (const auto &record : machine.getTransitionHistory()) {
                    GraphEdge edge;

                    if (record.fromState.empty()) {
                        edge.from = "start";
                        edge.to = record.toState;
                        edge.label = "initial";
                    } else {
                        edge.from = record.fromState;
                        edge.to = record.toState;
                        edge.label = record.reason;

                        // Color code by transition type
                        if (record.reason == "timed") {
                            edge.color = "blue";
                        } else if (record.reason == "weighted") {
                            edge.color = "purple";
                        } else if (record.reason == "probabilistic") {
                            edge.color = "orange";
                        }
                    }

                    // Avoid duplicate edges
                    std::string edgeKey = edge.from + "->" + edge.to;
                    if (addedTransitions.find(edgeKey) == addedTransitions.end()) {
                        graph.edges.push_back(edge);
                        addedTransitions.insert(edgeKey);
                    }
                }
            }

            return GraphvizExporter::toDot(graph);
        }

        // Export with full state machine structure (requires friend access or modified API)
        static std::string toFullDot(const std::string &graphName, const std::vector<state::StatePtr> &states,
                                     const std::vector<state::TransitionPtr> &transitions,
                                     const state::StatePtr &initialState,
                                     const state::StatePtr &currentState = nullptr) {

            Graph graph;
            graph.name = graphName;
            graph.rankdir = "LR";
            graph.attributes["node"] = "[shape=circle]";

            // Add initial state marker
            GraphNode startNode;
            startNode.id = "start";
            startNode.label = "";
            startNode.shape = "point";
            graph.nodes.push_back(startNode);

            // Add all states
            for (const auto &state : states) {
                GraphNode node;
                node.id = state->name();
                node.label = state->name();

                // Check if it's a composite state
                if (auto composite = std::dynamic_pointer_cast<state::CompositeState>(state)) {
                    node.shape = "doubleoctagon";
                } else {
                    node.shape = "circle";
                }

                // Highlight current state
                if (currentState && state->name() == currentState->name()) {
                    node.style = "filled";
                    node.fillcolor = "lightblue";
                } else if (initialState && state->name() == initialState->name()) {
                    node.style = "bold";
                }

                graph.nodes.push_back(node);
            }

            // Initial transition
            if (initialState) {
                GraphEdge edge;
                edge.from = "start";
                edge.to = initialState->name();
                edge.label = "";
                graph.edges.push_back(edge);
            }

            // Add all transitions
            for (const auto &transition : transitions) {
                GraphEdge edge;
                edge.from = transition->from()->name();
                edge.to = transition->to()->name();

                // Build label with transition info
                std::string label;

                if (transition->isTimedTransition()) {
                    label = "timed";
                    if (transition->getDuration().has_value()) {
                        label += "(" + std::to_string(transition->getDuration()->count()) + "ms)";
                    }
                    edge.color = "blue";
                } else if (transition->getWeight().has_value()) {
                    label = "w=" + std::to_string(transition->getWeight().value());
                    edge.color = "purple";
                } else if (transition->getProbability().has_value()) {
                    label = "p=" + std::to_string(transition->getProbability().value());
                    edge.color = "orange";
                } else {
                    label = "condition";
                }

                // Add priority if non-zero
                if (transition->getPriority() != 0) {
                    label += "\\npri=" + std::to_string(transition->getPriority());
                }

                edge.label = label;
                graph.edges.push_back(edge);
            }

            return GraphvizExporter::toDot(graph);
        }
    };

} // namespace stateup::visualization
