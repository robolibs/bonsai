#pragma once
#include "../structure/node.hpp"
#include <algorithm>
#include <functional>
#include <vector>

namespace bonsai::tree {

    // Utility node that selects child based on utility scores
    class UtilitySelector : public Node {
      public:
        using UtilityFunc = std::function<float(Blackboard &)>;

        struct UtilityChild {
            NodePtr node;
            UtilityFunc utilityFunc;

            UtilityChild(NodePtr n, UtilityFunc f) : node(std::move(n)), utilityFunc(std::move(f)) {}
        };

        inline void addChild(NodePtr child, UtilityFunc utilityFunc) {
            children_.emplace_back(std::move(child), std::move(utilityFunc));
        }

        inline Status tick(Blackboard &blackboard) override {
            if (halted_)
                return Status::Failure;
            if (children_.empty())
                return Status::Failure;

            // Calculate utilities for all children
            float maxUtility = -1.0f;
            size_t bestIndex = 0;

            for (size_t i = 0; i < children_.size(); ++i) {
                float utility = children_[i].utilityFunc(blackboard);
                if (utility > maxUtility) {
                    maxUtility = utility;
                    bestIndex = i;
                }
            }

            // If we switched to a different child, reset the previous one
            if (bestIndex != currentIndex_) {
                if (currentIndex_ < children_.size()) {
                    children_[currentIndex_].node->halt();
                }
                currentIndex_ = bestIndex;
            }

            return children_[currentIndex_].node->tick(blackboard);
        }

        inline void reset() override {
            halted_ = false;
            for (auto &child : children_) {
                child.node->reset();
            }
            currentIndex_ = SIZE_MAX; // Invalid index
        }

        inline void halt() override {
            halted_ = true;
            for (auto &child : children_) {
                child.node->halt();
            }
        }

      private:
        std::vector<UtilityChild> children_;
        size_t currentIndex_ = SIZE_MAX;
    };

    // Weighted random selector based on utility scores
    class WeightedRandomSelector : public Node {
      public:
        using UtilityFunc = std::function<float(Blackboard &)>;

        struct WeightedChild {
            NodePtr node;
            UtilityFunc weightFunc;

            WeightedChild(NodePtr n, UtilityFunc f) : node(std::move(n)), weightFunc(std::move(f)) {}
        };

        inline void addChild(NodePtr child, UtilityFunc weightFunc) {
            children_.emplace_back(std::move(child), std::move(weightFunc));
        }

        inline Status tick(Blackboard &blackboard) override {
            if (halted_)
                return Status::Failure;
            if (children_.empty())
                return Status::Failure;

            // Calculate total weight
            float totalWeight = 0.0f;
            std::vector<float> weights;
            weights.reserve(children_.size());

            for (auto &child : children_) {
                float weight = std::max(0.0f, child.weightFunc(blackboard));
                weights.push_back(weight);
                totalWeight += weight;
            }

            if (totalWeight <= 0.0f)
                return Status::Failure;

            // Select random child based on weights
            float random = static_cast<float>(rand()) / RAND_MAX * totalWeight;
            float accumulator = 0.0f;

            for (size_t i = 0; i < children_.size(); ++i) {
                accumulator += weights[i];
                if (random <= accumulator) {
                    return children_[i].node->tick(blackboard);
                }
            }

            // Fallback to last child
            return children_.back().node->tick(blackboard);
        }

        inline void reset() override {
            halted_ = false;
            for (auto &child : children_) {
                child.node->reset();
            }
        }

        inline void halt() override {
            halted_ = true;
            for (auto &child : children_) {
                child.node->halt();
            }
        }

      private:
        std::vector<WeightedChild> children_;
    };

} // namespace bonsai::tree