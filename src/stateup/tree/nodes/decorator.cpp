#include "stateup/tree/nodes/decorator.hpp"

namespace stateup::tree {

    Decorator::Decorator(Func func, NodePtr child) : func_(std::move(func)), child_(std::move(child)) {}

    Status Decorator::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;
        Status childStatus = child_->tick(blackboard);
        Status result = func_(childStatus);
        if (result != Status::Running) {
            state_ = State::Idle;
        }
        return result;
    }

    void Decorator::reset() {
        Node::reset();
        child_->reset();
    }

    void Decorator::halt() {
        Node::halt();
        child_->halt();
    }

    // RepeatDecorator implementation
    RepeatDecorator::RepeatDecorator(int maxTimes, NodePtr child)
        : maxTimes_(maxTimes), child_(std::move(child)), currentCount_(0) {}

    Status RepeatDecorator::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;

        Status childStatus = child_->tick(blackboard);

        if (childStatus == Status::Running)
            return Status::Running;

        if (childStatus == Status::Failure) {
            reset();
            return Status::Failure;
        }

        ++currentCount_;

        if (maxTimes_ > 0 && currentCount_ >= maxTimes_) {
            reset();
            return Status::Success;
        }

        child_->reset();
        return Status::Running;
    }

    void RepeatDecorator::reset() {
        Node::reset();
        currentCount_ = 0;
        child_->reset();
    }

    void RepeatDecorator::halt() {
        Node::halt();
        currentCount_ = 0;
        child_->halt();
    }

    // RetryDecorator implementation
    RetryDecorator::RetryDecorator(int maxTimes, NodePtr child)
        : maxTimes_(maxTimes), child_(std::move(child)), currentAttempts_(0) {}

    Status RetryDecorator::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;

        Status childStatus = child_->tick(blackboard);

        if (childStatus == Status::Running)
            return Status::Running;

        if (childStatus == Status::Success) {
            reset();
            return Status::Success;
        }

        ++currentAttempts_;
        child_->reset();

        if (maxTimes_ > 0 && currentAttempts_ >= maxTimes_) {
            reset();
            return Status::Failure;
        }

        return Status::Running;
    }

    void RetryDecorator::reset() {
        Node::reset();
        currentAttempts_ = 0;
        child_->reset();
    }

    void RetryDecorator::halt() {
        Node::halt();
        currentAttempts_ = 0;
        child_->halt();
    }

} // namespace stateup::tree
