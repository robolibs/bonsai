#pragma once
#include "../structure/node.hpp"
#include <vector>

namespace bonsai::tree {

    class Sequence : public Node {
      public:
        void addChild(const NodePtr &child);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<NodePtr> children_;
        size_t currentIndex_ = 0;
    };

} // namespace bonsai::tree
