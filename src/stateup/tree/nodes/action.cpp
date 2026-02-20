#include "stateup/tree/nodes/action.hpp"
#include <chrono>
#include <utility>

namespace stateup::tree {

    Action::Action(Func func) : func_(std::move(func)) {}
    Action::Action(AsyncFunc asyncFunc) : async_(std::move(asyncFunc)) {}
    Action::Action(TaskFunc taskFunc) : taskFunc_(std::move(taskFunc)) {}

    Status Action::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;

        // Prefer coroutine task if provided
        if (taskFunc_) {
            if (!task_.has_value()) {
                task_ = taskFunc_(blackboard);
            }
            // Advance the coroutine
            task_->resume();
            if (task_->done()) {
                auto result = task_->result();
                task_.reset();
                state_ = result == Status::Running ? State::Running : State::Idle;
                return result;
            }
            return Status::Running;
        }

        // Next, prefer async future if provided
        if (async_) {
            if (!pending_.valid()) {
                pending_ = async_(blackboard);
            }
            // Non-blocking poll using wait_for
            if (pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto result = pending_.get();
                state_ = result == Status::Running ? State::Running : State::Idle;
                return result;
            }
            return Status::Running;
        }

        Status result = func_(blackboard);
        if (result != Status::Running) {
            state_ = State::Idle;
        }
        return result;
    }

    void Action::reset() {
        Node::reset();
        if (pending_.valid()) {
            // No standard way to cancel std::future; let it complete in background
        }
        if (task_.has_value()) {
            task_.reset();
        }
    }

    void Action::halt() {
        Node::halt();
        if (task_.has_value()) {
            task_.reset();
        }
    }

} // namespace stateup::tree
