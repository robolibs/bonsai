#include "bonsai/tree/nodes/decorator.hpp"

namespace bonsai::tree {

    Decorator::Decorator(Func func, NodePtr child) : func_(std::move(func)), child_(std::move(child)) {}

    Status Decorator::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;
        Status childStatus = child_->tick(blackboard);
        return func_(childStatus);
    }

    void Decorator::reset() {
        halted_ = false;
        child_->reset();
    }

    void Decorator::halt() {
        halted_ = true;
        child_->halt();
    }

    // RepeatDecorator implementation
    RepeatDecorator::RepeatDecorator(int maxTimes, NodePtr child)
        : maxTimes_(maxTimes), child_(std::move(child)), currentCount_(0) {}

    Status RepeatDecorator::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;

        Status childStatus = child_->tick(blackboard);

        if (childStatus == Status::Running)
            return Status::Running;

        if (childStatus == Status::Failure) {
            currentCount_ = 0;
            return Status::Failure;
        }

        ++currentCount_;

        if (maxTimes_ > 0 && currentCount_ >= maxTimes_) {
            currentCount_ = 0;
            child_->reset();
            return Status::Success;
        }

        child_->reset();
        return Status::Running;
    }

    void RepeatDecorator::reset() {
        halted_ = false;
        currentCount_ = 0;
        child_->reset();
    }

    void RepeatDecorator::halt() {
        halted_ = true;
        currentCount_ = 0;
        child_->halt();
    }

    // RetryDecorator implementation
    RetryDecorator::RetryDecorator(int maxTimes, NodePtr child)
        : maxTimes_(maxTimes), child_(std::move(child)), currentAttempts_(0) {}

    Status RetryDecorator::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;

        Status childStatus = child_->tick(blackboard);

        if (childStatus == Status::Running)
            return Status::Running;

        if (childStatus == Status::Success) {
            currentAttempts_ = 0;
            return Status::Success;
        }

        ++currentAttempts_;
        child_->reset();

        if (maxTimes_ > 0 && currentAttempts_ >= maxTimes_) {
            currentAttempts_ = 0;
            return Status::Failure;
        }

        return Status::Running;
    }

    void RetryDecorator::reset() {
        halted_ = false;
        currentAttempts_ = 0;
        child_->reset();
    }

    void RetryDecorator::halt() {
        halted_ = true;
        currentAttempts_ = 0;
        child_->halt();
    }

} // namespace bonsai::tree
