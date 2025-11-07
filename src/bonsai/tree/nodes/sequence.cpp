#include "bonsai/tree/nodes/sequence.hpp"

namespace bonsai::tree {

    void Sequence::addChild(const NodePtr &child) { children_.emplace_back(child); }

    Status Sequence::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;
        while (currentIndex_ < children_.size()) {
            Status status = children_[currentIndex_]->tick(blackboard);
            if (status == Status::Running)
                return Status::Running;
            if (status == Status::Failure) {
                reset();
                return Status::Failure;
            }
            ++currentIndex_;
        }
        reset();
        return Status::Success;
    }

    void Sequence::reset() {
        Node::reset();
        currentIndex_ = 0;
        for (auto &child : children_) {
            child->reset();
        }
    }

    void Sequence::halt() {
        Node::halt();
        for (auto &child : children_) {
            child->halt();
        }
    }

} // namespace bonsai::tree
