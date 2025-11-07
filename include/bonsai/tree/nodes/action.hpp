#pragma once
#include "../structure/node.hpp"
#include <functional>

namespace bonsai::tree {

    class Action : public Node {
      public:
        using Func = std::function<Status(Blackboard &)>;

        explicit Action(Func func);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        Func func_;
    };

} // namespace bonsai::tree
