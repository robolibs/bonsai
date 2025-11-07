#include "bonsai/tree/nodes/parallel.hpp"

namespace bonsai::tree {

    Parallel::Parallel(Policy successPolicy, Policy failurePolicy)
        : successPolicy_(successPolicy), failurePolicy_(failurePolicy) {}

    void Parallel::addChild(const NodePtr &child) {
        children_.emplace_back(child);
        childStates_.emplace_back(Status::Idle);
    }

    Status Parallel::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;

        if (children_.empty())
            return Status::Success;

        size_t success = 0, failure = 0;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (childStates_[i] == Status::Success || childStates_[i] == Status::Failure) {
                if (childStates_[i] == Status::Success)
                    ++success;
                else
                    ++failure;
                continue;
            }

            Status status = children_[i]->tick(blackboard);
            childStates_[i] = status;

            if (status == Status::Success)
                ++success;
            else if (status == Status::Failure)
                ++failure;
        }

        bool successCondition = (successPolicy_ == Policy::RequireAll && success == children_.size()) ||
                                (successPolicy_ == Policy::RequireOne && success > 0);
        if (successCondition) {
            haltRunningChildren();
            reset();
            return Status::Success;
        }

        bool failureCondition = (failurePolicy_ == Policy::RequireAll && failure == children_.size()) ||
                                (failurePolicy_ == Policy::RequireOne && failure > 0);
        if (failureCondition) {
            haltRunningChildren();
            reset();
            return Status::Failure;
        }

        return Status::Running;
    }

    void Parallel::reset() {
        Node::reset();
        for (size_t i = 0; i < children_.size(); ++i) {
            childStates_[i] = Status::Idle;
            children_[i]->reset();
        }
    }

    void Parallel::halt() {
        Node::halt();
        haltRunningChildren();
        for (size_t i = 0; i < children_.size(); ++i) {
            children_[i]->reset();
            childStates_[i] = Status::Idle;
        }
    }

    void Parallel::haltRunningChildren() {
        for (size_t i = 0; i < children_.size(); ++i) {
            if (childStates_[i] == Status::Running) {
                children_[i]->halt();
                childStates_[i] = Status::Idle;
            }
        }
    }

} // namespace bonsai::tree
