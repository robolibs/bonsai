#include "stateup/tree/nodes/sequence.hpp"

namespace stateup::tree {

    void Sequence::addChild(const NodePtr &child) { children_.emplace_back(child); }

    Status Sequence::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        // FIX: Remember running state - don't reset state on re-entry

        state_ = State::Running;

        while (currentIndex_ < children_.size()) {
            Status status = children_[currentIndex_]->tick(blackboard);
            if (status == Status::Running) {
                // Child is still running, remember where we are
                return Status::Running;
            }
            if (status == Status::Failure) {
                // Only reset on failure, not on running

                reset();
                return Status::Failure;
            }
            ++currentIndex_;
        }
        // Only reset on completion
        reset();
        return Status::Success;
    }

    void Sequence::reset() {
        Node::reset();
        // FIX: Only reset children that were actually executed
        for (size_t i = 0; i < currentIndex_ && i < children_.size(); ++i) {
            children_[i]->reset();
        }
        // Reset the current running child if any
        if (currentIndex_ < children_.size()) {
            children_[currentIndex_]->reset();
        }
        currentIndex_ = 0;
    }

    void Sequence::halt() {
        Node::halt();
        for (auto &child : children_) {
            child->halt();
        }
    }

} // namespace stateup::tree
