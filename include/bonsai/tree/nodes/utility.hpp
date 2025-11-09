#pragma once
#include "../structure/node.hpp"
#include <algorithm>
#include <functional>
#include <vector>

namespace bonsai::tree {

    // Utility node that selects child based on utility scores
    class UtilitySelector : public Node {
      public:
        using UtilityFunc = std::function<float(Blackboard &)>;

        struct UtilityChild {
            NodePtr node;
            UtilityFunc utilityFunc;

            UtilityChild(NodePtr n, UtilityFunc f);
        };

        void addChild(NodePtr child, UtilityFunc utilityFunc);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<UtilityChild> children_;
        size_t currentIndex_ = SIZE_MAX;
    };

    // Weighted random selector based on utility scores
    class WeightedRandomSelector : public Node {
      public:
        using UtilityFunc = std::function<float(Blackboard &)>;

        struct WeightedChild {
            NodePtr node;
            UtilityFunc weightFunc;

            WeightedChild(NodePtr n, UtilityFunc f);
        };

        void addChild(NodePtr child, UtilityFunc weightFunc);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<WeightedChild> children_;
        size_t currentIndex_ = SIZE_MAX;
    };

} // namespace bonsai::tree
