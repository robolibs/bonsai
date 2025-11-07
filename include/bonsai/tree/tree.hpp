#pragma once
#include "structure/blackboard.hpp"
#include "structure/node.hpp"

namespace bonsai::tree {

    class Tree {
      public:
        explicit Tree(NodePtr root) : root_(std::move(root)) {}

        inline Status tick() {
            if (!root_)
                return Status::Failure;
            return root_->tick(blackboard_);
        }

        inline void reset() {
            if (root_)
                root_->reset();
        }

        inline void halt() {
            if (root_)
                root_->halt();
        }

        inline Blackboard &blackboard() { return blackboard_; }
        inline const Blackboard &blackboard() const { return blackboard_; }

        inline NodePtr getRoot() const { return root_; }

      private:
        NodePtr root_;
        Blackboard blackboard_;
    };

    // Alias for backward compatibility with howto.md examples
    using Bonsai = Tree;

} // namespace bonsai::tree