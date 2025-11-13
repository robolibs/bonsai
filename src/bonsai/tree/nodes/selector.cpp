#include "bonsai/tree/nodes/selector.hpp"

namespace bonsai::tree {

    void Selector::addChild(const NodePtr &child) { children_.emplace_back(child); }

    Status Selector::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;
        while (currentIndex_ < children_.size()) {
            Status status = children_[currentIndex_]->tick(blackboard);
            if (status == Status::Running)
                return Status::Running;
            if (status == Status::Success) {
                reset();
                return Status::Success;
            }
            ++currentIndex_;
        }
        reset();
        return Status::Failure;
    }

    void Selector::reset() {
        Node::reset();
        // FIX: Only reset children that were actually tried
        for (size_t i = 0; i < currentIndex_ && i < children_.size(); ++i) {
            children_[i]->reset();
        }
        // Reset the current running child if any
        if (currentIndex_ < children_.size()) {
            children_[currentIndex_]->reset();
        }
        currentIndex_ = 0;
    }

    void Selector::halt() {
        Node::halt();
        for (auto &child : children_) {
            child->halt();
        }
    }

} // namespace bonsai::tree
