#pragma once
#include "structure/blackboard.hpp"
#include "structure/node.hpp"

namespace bonsai::tree {

    class Tree {
      public:
        explicit Tree(NodePtr root);

        Status tick();
        void reset();
        void halt();

        Blackboard &blackboard();
        const Blackboard &blackboard() const;
        NodePtr getRoot() const;

      private:
        NodePtr root_;
        Blackboard blackboard_;
    };

    // Alias for backward compatibility with howto.md examples
    using Bonsai = Tree;

} // namespace bonsai::tree
