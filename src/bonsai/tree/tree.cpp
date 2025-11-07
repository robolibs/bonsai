#include "bonsai/tree/tree.hpp"

namespace bonsai::tree {

    Tree::Tree(NodePtr root) : root_(std::move(root)) {}

    Status Tree::tick() {
        if (!root_)
            return Status::Failure;
        return root_->tick(blackboard_);
    }

    void Tree::reset() {
        if (root_)
            root_->reset();
    }

    void Tree::halt() {
        if (root_)
            root_->halt();
    }

    Blackboard &Tree::blackboard() { return blackboard_; }

    const Blackboard &Tree::blackboard() const { return blackboard_; }

    NodePtr Tree::getRoot() const { return root_; }

} // namespace bonsai::tree
