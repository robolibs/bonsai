#include "bonsai/tree/tree.hpp"

namespace bonsai::tree {

    Tree::Tree(NodePtr root) : root_(std::move(root)) {}

    Status Tree::tick() {
        if (!root_)
            return Status::Failure;
        if (root_->isHalted())
            root_->reset();
        return root_->tick(blackboard_);
    }

    void Tree::reset() {
        if (root_)
            root_->reset();
    }

    void Tree::halt() {
        if (!root_)
            return;
        root_->halt();
        root_->reset();
    }

    Blackboard &Tree::blackboard() { return blackboard_; }

    const Blackboard &Tree::blackboard() const { return blackboard_; }

    NodePtr Tree::getRoot() const { return root_; }

} // namespace bonsai::tree
