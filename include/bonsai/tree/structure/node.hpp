#pragma once
#include "blackboard.hpp"
#include "status.hpp"
#include <memory>

namespace bonsai::tree {

    class Node {
      public:
        virtual ~Node() = default;
        virtual Status tick(Blackboard &blackboard) = 0;
        virtual void reset() {}
        virtual void halt() {}

        inline bool isHalted() const { return halted_; }

      protected:
        bool halted_ = false;
    };

    using NodePtr = std::shared_ptr<Node>;

} // namespace bonsai::tree