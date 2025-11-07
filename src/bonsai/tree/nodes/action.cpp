#include "bonsai/tree/nodes/action.hpp"

namespace bonsai::tree {

    Action::Action(Func func) : func_(std::move(func)) {}

    Status Action::tick(Blackboard &blackboard) {
        if (halted_)
            return Status::Failure;
        return func_(blackboard);
    }

    void Action::reset() { halted_ = false; }

    void Action::halt() { halted_ = true; }

} // namespace bonsai::tree
