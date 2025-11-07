#include "bonsai/tree/nodes/action.hpp"

namespace bonsai::tree {

    Action::Action(Func func) : func_(std::move(func)) {}

    Status Action::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;
        Status result = func_(blackboard);
        if (result != Status::Running) {
            state_ = State::Idle;
        }
        return result;
    }

    void Action::reset() { Node::reset(); }

    void Action::halt() { Node::halt(); }

} // namespace bonsai::tree
