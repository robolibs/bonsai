#pragma once
#include "../../tree/structure/blackboard.hpp"
#include "state.hpp"
#include <functional>
#include <memory>
#include <string>

namespace bonsai::state {

    // Callback-based state for easy lambda usage
    class CbState : public State {
      public:
        explicit CbState(std::string name) : State(std::move(name)) {}

        // Create a callback state with just an update function
        static std::shared_ptr<CbState> create(const std::string &name, Func updateFunc) {
            auto state = std::make_shared<CbState>(name);
            state->setOnUpdate(std::move(updateFunc));
            return state;
        }

        // Create a callback state with all lifecycle functions
        static std::shared_ptr<CbState> create(const std::string &name, GuardFunc guardFunc, Func enterFunc,
                                               Func updateFunc, Func exitFunc) {
            auto state = std::make_shared<CbState>(name);
            if (guardFunc)
                state->setOnGuard(std::move(guardFunc));
            if (enterFunc)
                state->setOnEnter(std::move(enterFunc));
            if (updateFunc)
                state->setOnUpdate(std::move(updateFunc));
            if (exitFunc)
                state->setOnExit(std::move(exitFunc));
            return state;
        }
    };

    using CbStatePtr = std::shared_ptr<CbState>;

} // namespace bonsai::state
