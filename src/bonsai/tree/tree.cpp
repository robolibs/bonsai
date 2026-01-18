#include "bonsai/tree/tree.hpp"
#include "bonsai/tree/events.hpp"

namespace bonsai::tree {

    Tree::Tree(NodePtr root) : root_(std::move(root)), eventBus_(std::make_shared<EventBus>()) {}

    Status Tree::tick() {
        if (!root_)
            return Status::Failure;

        if (root_->state() == Node::State::Halted)
            root_->reset();

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

    EventBus &Tree::events() { return *eventBus_; }

    const EventBus &Tree::events() const { return *eventBus_; }

} // namespace bonsai::tree
