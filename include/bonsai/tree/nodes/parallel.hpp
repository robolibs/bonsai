#pragma once
#include "../structure/node.hpp"
#include <vector>

namespace bonsai::tree {

    class Parallel : public Node {
      public:
        enum class Policy { RequireAll, RequireOne };

        Parallel(Policy successPolicy, Policy failurePolicy);

        void addChild(const NodePtr &child);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<NodePtr> children_;
        Policy successPolicy_, failurePolicy_;
    };

} // namespace bonsai::tree
