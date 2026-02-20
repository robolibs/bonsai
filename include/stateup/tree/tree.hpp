#pragma once
#include "structure/blackboard.hpp"
#include "structure/node.hpp"
#include <memory>

// Forward declare EventBus to avoid heavy include
namespace stateup::tree {
    class EventBus;
}

namespace stateup::tree {

    class Tree {
      public:
        explicit Tree(NodePtr root);

        Status tick();
        void reset();
        void halt();

        Blackboard &blackboard();
        const Blackboard &blackboard() const;
        NodePtr getRoot() const;

        // Event bus access
        EventBus &events();
        const EventBus &events() const;

      private:
        NodePtr root_;
        Blackboard blackboard_;
        std::shared_ptr<EventBus> eventBus_;
    };

    // Alias for backward compatibility with howto.md examples
    using Stateup = Tree;

} // namespace stateup::tree
