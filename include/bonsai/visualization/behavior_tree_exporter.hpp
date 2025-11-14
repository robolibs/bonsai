#pragma once
#include "../tree/structure/node.hpp"
#include "../tree/tree.hpp"
#include "graphviz.hpp"
#include <memory>

namespace bonsai::visualization {

    class BehaviorTreeExporter {
      public:
        // Export behavior tree to Graphviz DOT format
        static std::string toDot(const tree::Tree &behaviorTree, const std::string &graphName = "BehaviorTree") {
            Graph graph;
            graph.name = graphName;
            graph.rankdir = "TB"; // Top to bottom for trees

            auto root = behaviorTree.getRoot();
            if (!root) {
                return "digraph " + graphName + " {\n  // Empty tree\n}\n";
            }

            int nodeCounter = 0;
            exportNodeRecursive(graph, root, "", nodeCounter);

            return GraphvizExporter::toDot(graph);
        }

      private:
        static std::string exportNodeRecursive(Graph &graph, const tree::NodePtr &node, const std::string &parentId,
                                               int &nodeCounter) {
            if (!node) {
                return "";
            }

            // Generate unique ID for this node
            std::string nodeId = "node" + std::to_string(nodeCounter++);

            // Determine node type and appearance based on class name
            GraphNode graphNode;
            graphNode.id = nodeId;
            graphNode.label = getNodeTypeName(node);

            // Set shape based on node type
            if (isCompositeNode(node)) {
                graphNode.shape = "box";
                graphNode.style = "rounded,filled";
                graphNode.fillcolor = "lightgray";
            } else if (isDecoratorNode(node)) {
                graphNode.shape = "diamond";
                graphNode.style = "filled";
                graphNode.fillcolor = "lightyellow";
            } else {
                // Leaf node (action, condition)
                graphNode.shape = "ellipse";
                graphNode.style = "filled";
                graphNode.fillcolor = "lightgreen";
            }

            // Highlight running nodes
            if (node->state() == tree::Node::State::Running) {
                graphNode.fillcolor = "lightblue";
                graphNode.attributes["penwidth"] = "3";
            } else if (node->state() == tree::Node::State::Halted) {
                graphNode.fillcolor = "lightcoral";
            }

            graph.nodes.push_back(graphNode);

            // Add edge from parent to this node
            if (!parentId.empty()) {
                GraphEdge edge;
                edge.from = parentId;
                edge.to = nodeId;
                graph.edges.push_back(edge);
            }

            // Recursively export children
            // Note: This requires access to node's children, which we don't have
            // in the current API. This is a placeholder for when we add child access.

            return nodeId;
        }

        static std::string getNodeTypeName(const tree::NodePtr &node) {
            // Use RTTI to get type name
            // This is a simplified version - in practice, we'd need to check actual types
            const char *typeName = typeid(*node).name();
            std::string name = typeName;

            // Demangle C++ type names (simplified)
            size_t pos = name.find_last_of(':');
            if (pos != std::string::npos) {
                name = name.substr(pos + 1);
            }

            // Remove numeric prefix from GCC mangled names
            while (!name.empty() && std::isdigit(name[0])) {
                name = name.substr(1);
            }

            return name.empty() ? "Node" : name;
        }

        static bool isCompositeNode(const tree::NodePtr &node) {
            // Check if node is a composite (Sequence, Selector, Parallel, etc.)
            // This is a heuristic based on common naming conventions
            std::string typeName = getNodeTypeName(node);
            return typeName.find("Sequence") != std::string::npos || typeName.find("Selector") != std::string::npos ||
                   typeName.find("Parallel") != std::string::npos;
        }

        static bool isDecoratorNode(const tree::NodePtr &node) {
            // Check if node is a decorator (Inverter, Repeater, etc.)
            std::string typeName = getNodeTypeName(node);
            return typeName.find("Decorator") != std::string::npos || typeName.find("Inverter") != std::string::npos ||
                   typeName.find("Repeat") != std::string::npos || typeName.find("Retry") != std::string::npos ||
                   typeName.find("Timeout") != std::string::npos || typeName.find("Cooldown") != std::string::npos;
        }
    };

} // namespace bonsai::visualization
