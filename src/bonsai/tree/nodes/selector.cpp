#include "bonsai/tree/nodes/selector.hpp"

namespace bonsai::tree {

    void Selector::addChild(const NodePtr &child) { children_.emplace_back(child); }

    Status Selector::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;

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
        currentIndex_ = 0;
        halted_ = false;
        for (auto &child : children_) {
            child->reset();
        }
    }

    void Selector::halt() {
        halted_ = true;
        for (auto &child : children_) {
            child->halt();
        }
    }

} // namespace bonsai::tree
