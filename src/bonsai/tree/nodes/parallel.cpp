#include "bonsai/tree/nodes/parallel.hpp"

namespace bonsai::tree {

    Parallel::Parallel(Policy successPolicy, Policy failurePolicy)
        : successPolicy_(successPolicy), failurePolicy_(failurePolicy) {}

    void Parallel::addChild(const NodePtr &child) { children_.emplace_back(child); }

    Status Parallel::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;

        size_t success = 0, failure = 0;
        for (auto &child : children_) {
            Status status = child->tick(blackboard);
            if (status == Status::Success)
                ++success;
            else if (status == Status::Failure)
                ++failure;
        }

        if ((successPolicy_ == Policy::RequireAll && success == children_.size()) ||
            (successPolicy_ == Policy::RequireOne && success > 0)) {
            return Status::Success;
        }

        if ((failurePolicy_ == Policy::RequireAll && failure == children_.size()) ||
            (failurePolicy_ == Policy::RequireOne && failure > 0)) {
            return Status::Failure;
        }

        return Status::Running;
    }

    void Parallel::reset() {
        halted_ = false;
        for (auto &child : children_) {
            child->reset();
        }
    }

    void Parallel::halt() {
        halted_ = true;
        for (auto &child : children_) {
            child->halt();
        }
    }

} // namespace bonsai::tree
