#pragma once
#include "../structure/node.hpp"
#include "bonsai/core/task.hpp"
#include <functional>
#include <future>
#include <optional>

namespace bonsai::tree {

    class Action : public Node {
      public:
        using Func = std::function<Status(Blackboard &)>;
        using AsyncFunc = std::function<std::future<Status>(Blackboard &)>;
        using TaskFunc = std::function<bonsai::core::task<Status>(Blackboard &)>;

        explicit Action(Func func);
        explicit Action(AsyncFunc asyncFunc);
        explicit Action(TaskFunc taskFunc);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        Func func_;
        AsyncFunc async_;
        TaskFunc taskFunc_;
        std::future<Status> pending_;
        std::optional<bonsai::core::task<Status>> task_;
    };

} // namespace bonsai::tree
