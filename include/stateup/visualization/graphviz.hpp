#pragma once
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace stateup::visualization {

    // Common graph node representation
    struct GraphNode {
        std::string id;
        std::string label;
        std::string shape = "circle";
        std::string style = "";
        std::string fillcolor = "";
        std::string color = "black";
        std::unordered_map<std::string, std::string> attributes;
    };

    // Common graph edge representation
    struct GraphEdge {
        std::string from;
        std::string to;
        std::string label = "";
        std::string style = "";
        std::string color = "black";
        std::unordered_map<std::string, std::string> attributes;
    };

    // Cluster/subgraph for hierarchical structures
    struct GraphCluster {
        std::string id;
        std::string label;
        std::vector<std::string> nodeIds;
        std::vector<GraphCluster> subclusters;
        std::string style = "rounded";
        std::unordered_map<std::string, std::string> attributes;
    };

    // Main graph structure
    struct Graph {
        std::string name;
        std::string rankdir = "TB"; // TB = top-to-bottom, LR = left-to-right
        std::vector<GraphNode> nodes;
        std::vector<GraphEdge> edges;
        std::vector<GraphCluster> clusters;
        std::unordered_map<std::string, std::string> attributes;
    };

    // Graphviz DOT exporter
    class GraphvizExporter {
      public:
        static std::string toDot(const Graph &graph) {
            std::ostringstream oss;

            oss << "digraph " << escapeId(graph.name) << " {\n";
            oss << "  rankdir=" << graph.rankdir << ";\n";

            // Global graph attributes
            for (const auto &[key, value] : graph.attributes) {
                oss << "  " << key << "=" << escapeValue(value) << ";\n";
            }

            if (!graph.attributes.empty()) {
                oss << "  \n";
            }

            // Export clusters first (bottom-up for proper nesting)
            for (const auto &cluster : graph.clusters) {
                exportCluster(oss, cluster, 1);
            }

            // Export nodes
            std::unordered_set<std::string> clusterNodes;
            collectClusterNodes(graph.clusters, clusterNodes);

            for (const auto &node : graph.nodes) {
                // Skip nodes that are in clusters (they're exported with the cluster)
                if (clusterNodes.find(node.id) != clusterNodes.end()) {
                    continue;
                }
                exportNode(oss, node, 1);
            }

            if (!graph.nodes.empty()) {
                oss << "  \n";
            }

            // Export edges
            for (const auto &edge : graph.edges) {
                exportEdge(oss, edge, 1);
            }

            oss << "}\n";
            return oss.str();
        }

      private:
        static void exportCluster(std::ostringstream &oss, const GraphCluster &cluster, int indent) {
            std::string indentStr(indent * 2, ' ');

            oss << indentStr << "subgraph cluster_" << escapeId(cluster.id) << " {\n";
            oss << indentStr << "  label=" << escapeValue(cluster.label) << ";\n";
            oss << indentStr << "  style=" << cluster.style << ";\n";

            // Cluster attributes
            for (const auto &[key, value] : cluster.attributes) {
                oss << indentStr << "  " << key << "=" << escapeValue(value) << ";\n";
            }

            oss << indentStr << "  \n";

            // Export subclusters
            for (const auto &subcluster : cluster.subclusters) {
                exportCluster(oss, subcluster, indent + 1);
            }

            // Export nodes in this cluster (just IDs, full definitions come later)
            for (const auto &nodeId : cluster.nodeIds) {
                oss << indentStr << "  " << escapeId(nodeId) << ";\n";
            }

            oss << indentStr << "}\n";
        }

        static void exportNode(std::ostringstream &oss, const GraphNode &node, int indent) {
            std::string indentStr(indent * 2, ' ');

            oss << indentStr << escapeId(node.id) << " [";

            std::vector<std::string> attrs;
            attrs.push_back("label=" + escapeValue(node.label));
            attrs.push_back("shape=" + node.shape);

            if (!node.style.empty()) {
                attrs.push_back("style=" + node.style);
            }
            if (!node.fillcolor.empty()) {
                attrs.push_back("fillcolor=" + node.fillcolor);
            }
            if (node.color != "black") {
                attrs.push_back("color=" + node.color);
            }

            for (const auto &[key, value] : node.attributes) {
                attrs.push_back(key + "=" + escapeValue(value));
            }

            for (size_t i = 0; i < attrs.size(); ++i) {
                if (i > 0)
                    oss << ",";
                oss << attrs[i];
            }

            oss << "];\n";
        }

        static void exportEdge(std::ostringstream &oss, const GraphEdge &edge, int indent) {
            std::string indentStr(indent * 2, ' ');

            oss << indentStr << escapeId(edge.from) << " -> " << escapeId(edge.to);

            std::vector<std::string> attrs;
            if (!edge.label.empty()) {
                attrs.push_back("label=" + escapeValue(edge.label));
            }
            if (!edge.style.empty()) {
                attrs.push_back("style=" + edge.style);
            }
            if (edge.color != "black") {
                attrs.push_back("color=" + edge.color);
            }

            for (const auto &[key, value] : edge.attributes) {
                attrs.push_back(key + "=" + escapeValue(value));
            }

            if (!attrs.empty()) {
                oss << " [";
                for (size_t i = 0; i < attrs.size(); ++i) {
                    if (i > 0)
                        oss << ",";
                    oss << attrs[i];
                }
                oss << "]";
            }

            oss << ";\n";
        }

        static void collectClusterNodes(const std::vector<GraphCluster> &clusters,
                                        std::unordered_set<std::string> &nodeIds) {
            for (const auto &cluster : clusters) {
                for (const auto &nodeId : cluster.nodeIds) {
                    nodeIds.insert(nodeId);
                }
                collectClusterNodes(cluster.subclusters, nodeIds);
            }
        }

        static std::string escapeId(const std::string &str) {
            // Simple escaping - wrap in quotes if needed
            bool needsQuotes = false;
            for (char c : str) {
                if (!std::isalnum(c) && c != '_') {
                    needsQuotes = true;
                    break;
                }
            }

            if (needsQuotes) {
                return "\"" + escapeString(str) + "\"";
            }
            return str;
        }

        static std::string escapeValue(const std::string &str) { return "\"" + escapeString(str) + "\""; }

        static std::string escapeString(const std::string &str) {
            std::string result;
            result.reserve(str.size());
            for (char c : str) {
                if (c == '"' || c == '\\') {
                    result += '\\';
                }
                result += c;
            }
            return result;
        }
    };

} // namespace stateup::visualization
