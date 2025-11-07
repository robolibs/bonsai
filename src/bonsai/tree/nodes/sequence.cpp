#include "bonsai/tree/nodes/sequence.hpp"

namespace bonsai::tree {

    void Sequence::addChild(const NodePtr &child) { children_.emplace_back(child); }

    Status Sequence::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;

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
        currentIndex_ = 0;
        halted_ = false;
        for (auto &child : children_) {
            child->reset();
        }
    }

    void Sequence::halt() {
        halted_ = true;
        for (auto &child : children_) {
            child->halt();
        }
    }

} // namespace bonsai::tree
