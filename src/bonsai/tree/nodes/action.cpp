#include "bonsai/tree/nodes/action.hpp"
#include <chrono>
#include <utility>

namespace bonsai::tree {

    Action::Action(Func func) : func_(std::move(func)) {}
    Action::Action(AsyncFunc asyncFunc) : async_(std::move(asyncFunc)) {}

    Status Action::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;

        // Prefer async if provided
        if (async_) {
            if (!pending_.valid()) {
                pending_ = async_(blackboard);
            }
            // Non-blocking poll using wait_for
            if (pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto result = pending_.get();
                state_ = Status::Running == result ? State::Running : State::Idle;
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
    }

    void Action::halt() { Node::halt(); }

} // namespace bonsai::tree
