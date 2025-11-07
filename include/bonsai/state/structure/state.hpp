#pragma once
#include "../../tree/structure/blackboard.hpp"
#include <functional>
#include <memory>
#include <string>

namespace bonsai::state {

    class State {
      public:
        using Func = std::function<void(tree::Blackboard &)>;

        explicit State(std::string name) : name_(std::move(name)) {}
        virtual ~State() = default;

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
        void setOnEnter(Func func) { onEnterFunc_ = std::move(func); }
        void setOnUpdate(Func func) { onUpdateFunc_ = std::move(func); }
        void setOnExit(Func func) { onExitFunc_ = std::move(func); }

        // Getters
        const std::string &name() const { return name_; }

      protected:
        std::string name_;
        Func onEnterFunc_;
        Func onUpdateFunc_;
        Func onExitFunc_;
    };

    using StatePtr = std::shared_ptr<State>;

} // namespace bonsai::state
