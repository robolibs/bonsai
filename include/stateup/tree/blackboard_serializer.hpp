#pragma once
#include "structure/blackboard.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace stateup::tree {

    class BlackboardSerializer {
      public:
        // Save blackboard to JSON-like format
        static std::string serialize(const Blackboard &blackboard) {
            std::stringstream ss;
            ss << "{\n";

            auto keys = blackboard.getAllKeys();
            for (size_t i = 0; i < keys.size(); ++i) {
                const auto &key = keys[i];
                auto typeInfo = blackboard.getType(key);

                ss << "  \"" << key << "\": {";
                ss << "\"type\": \"" << (typeInfo ? typeInfo->name() : "unknown") << "\"";

                // Try to get common types
                if (auto val = blackboard.get<int>(key)) {
                    ss << ", \"value\": " << *val;
                } else if (auto val = blackboard.get<float>(key)) {
                    ss << ", \"value\": " << *val;
                } else if (auto val = blackboard.get<double>(key)) {
                    ss << ", \"value\": " << *val;
                } else if (auto val = blackboard.get<bool>(key)) {
                    ss << ", \"value\": " << (*val ? "true" : "false");
                } else if (auto val = blackboard.get<std::string>(key)) {
                    ss << ", \"value\": \"" << *val << "\"";
                } else {
                    ss << ", \"value\": \"<complex_type>\"";
                }

                ss << "}";
                if (i < keys.size() - 1)
                    ss << ",";
                ss << "\n";
            }

            ss << "}\n";
            return ss.str();
        }

        // Save to file
        static bool saveToFile(const Blackboard &blackboard, const std::string &filename) {
            std::ofstream file(filename);
            if (!file.is_open())
                return false;

            file << serialize(blackboard);
            file.close();
            return true;
        }

        // Simple deserialization for basic types
        static void deserialize(Blackboard &blackboard, const std::string &data) {
            // This is a simplified implementation
            // A full implementation would need a proper JSON parser
            blackboard.clear();

            // Basic parsing logic would go here
            // For now, this is a placeholder
        }

        // Load from file
        static bool loadFromFile(Blackboard &blackboard, const std::string &filename) {
            std::ifstream file(filename);
            if (!file.is_open())
                return false;

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            deserialize(blackboard, buffer.str());
            return true;
        }
    };

} // namespace stateup::tree