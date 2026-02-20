#pragma once
#include "../../tree/structure/blackboard.hpp"
#include <functional>
#include <memory>
#include <string>

namespace stateup::state {

    class State {
      public:
        using Func = std::function<void(tree::Blackboard &)>;
        using GuardFunc = std::function<bool(tree::Blackboard &)>;

        explicit State(std::string name) : name_(std::move(name)) {}
        virtual ~State() = default;

        // Guard condition - returns true if state should execute
        virtual bool onGuard(tree::Blackboard &blackboard) {
            if (onGuardFunc_)
                return onGuardFunc_(blackboard);
            return true; // Default: allow transition
        }

        // State lifecycle callbacks
        virtual void onEnter(tree::Blackboard &blackboard) {
            if (onEnterFunc_)
                onEnterFunc_(blackboard);
        }

        virtual void onUpdate(tree::Blackboard &blackboard) {
            if (onUpdateFunc_)
                onUpdateFunc_(blackboard);
        }

        virtual void onExit(tree::Blackboard &blackboard) {
            if (onExitFunc_)
                onExitFunc_(blackboard);
        }

        // Setters for callbacks
        void setOnGuard(GuardFunc func) { onGuardFunc_ = std::move(func); }
        void setOnEnter(Func func) { onEnterFunc_ = std::move(func); }
        void setOnUpdate(Func func) { onUpdateFunc_ = std::move(func); }
        void setOnExit(Func func) { onExitFunc_ = std::move(func); }

        // Getters
        const std::string &name() const { return name_; }

      protected:
        std::string name_;
        GuardFunc onGuardFunc_;
        Func onEnterFunc_;
        Func onUpdateFunc_;
        Func onExitFunc_;
    };

    using StatePtr = std::shared_ptr<State>;

} // namespace stateup::state
