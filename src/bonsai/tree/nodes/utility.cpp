#include "bonsai/tree/nodes/utility.hpp"
#include <cstdlib>

namespace bonsai::tree {

    // UtilitySelector implementation
    UtilitySelector::UtilityChild::UtilityChild(NodePtr n, UtilityFunc f)
        : node(std::move(n)), utilityFunc(std::move(f)) {}

    void UtilitySelector::addChild(NodePtr child, UtilityFunc utilityFunc) {
        children_.emplace_back(std::move(child), std::move(utilityFunc));
    }

    Status UtilitySelector::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;
        if (children_.empty())
            return Status::Failure;

        state_ = State::Running;

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

        Status result = children_[currentIndex_].node->tick(blackboard);
        if (result != Status::Running)
            state_ = State::Idle;

        return result;
    }

    void UtilitySelector::reset() {
        Node::reset();
        for (auto &child : children_) {
            child.node->reset();
        }
        currentIndex_ = SIZE_MAX; // Invalid index
    }

    void UtilitySelector::halt() {
        Node::halt();
        for (auto &child : children_) {
            child.node->halt();
        }
    }

    // WeightedRandomSelector implementation
    WeightedRandomSelector::WeightedChild::WeightedChild(NodePtr n, UtilityFunc f)
        : node(std::move(n)), weightFunc(std::move(f)) {}

    void WeightedRandomSelector::addChild(NodePtr child, UtilityFunc weightFunc) {
        children_.emplace_back(std::move(child), std::move(weightFunc));
    }

    Status WeightedRandomSelector::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;
        if (children_.empty())
            return Status::Failure;

        state_ = State::Running;

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
                Status result = children_[i].node->tick(blackboard);
                if (result != Status::Running)
                    state_ = State::Idle;
                return result;
            }
        }

        // Fallback to last child
        Status result = children_.back().node->tick(blackboard);
        if (result != Status::Running)
            state_ = State::Idle;
        return result;
    }

    void WeightedRandomSelector::reset() {
        Node::reset();
        for (auto &child : children_) {
            child.node->reset();
        }
    }

    void WeightedRandomSelector::halt() {
        Node::halt();
        for (auto &child : children_) {
            child.node->halt();
        }
    }

} // namespace bonsai::tree
