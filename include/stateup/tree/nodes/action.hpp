#pragma once
#include "../structure/node.hpp"
#include "stateup/core/task.hpp"
#include <functional>
#include <future>
#include <optional>

namespace stateup::tree {

    class Action : public Node {
      public:
        using Func = std::function<Status(Blackboard &)>;
        using AsyncFunc = std::function<std::future<Status>(Blackboard &)>;
        using TaskFunc = std::function<stateup::core::task<Status>(Blackboard &)>;

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
        std::optional<stateup::core::task<Status>> task_;
    };

} // namespace stateup::tree
