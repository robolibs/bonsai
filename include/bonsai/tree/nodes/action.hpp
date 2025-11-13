#pragma once
#include "../structure/node.hpp"
#include <functional>
#include <future>

namespace bonsai::tree {

    class Action : public Node {
      public:
        using Func = std::function<Status(Blackboard &)>;
        using AsyncFunc = std::function<std::future<Status>(Blackboard &)>;

        explicit Action(Func func);
        explicit Action(AsyncFunc asyncFunc);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        Func func_;
        AsyncFunc async_;
        std::future<Status> pending_;
    };

} // namespace bonsai::tree
