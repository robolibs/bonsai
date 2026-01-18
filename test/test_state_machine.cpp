#include "doctest/doctest.h"

#include "bonsai/state/state.hpp"
#include <string>

using namespace bonsai;

// Test states for basic functionality
class TestStateA : public state::State {
  public:
    TestStateA() : State("StateA") {}

    void onEnter(tree::Blackboard &blackboard) override {
        State::onEnter(blackboard);
        blackboard.set<int>("enter_count", blackboard.get<int>("enter_count").value_or(0) + 1);
    }

    void onUpdate(tree::Blackboard &blackboard) override {
        State::onUpdate(blackboard);
        blackboard.set<int>("update_count", blackboard.get<int>("update_count").value_or(0) + 1);
    }

    void onExit(tree::Blackboard &blackboard) override {
        State::onExit(blackboard);
        blackboard.set<int>("exit_count", blackboard.get<int>("exit_count").value_or(0) + 1);
    }
};

class TestStateB : public state::State {
  public:
    TestStateB() : State("StateB") {}

    void onEnter(tree::Blackboard &blackboard) override {
        State::onEnter(blackboard);
        blackboard.set<std::string>("last_state", "B");
    }

    void onUpdate(tree::Blackboard &blackboard) override {
        State::onUpdate(blackboard);
        auto counter = blackboard.get<int>("b_counter").value_or(0);
        blackboard.set<int>("b_counter", counter + 1);
    }
};

TEST_CASE("State: Basic creation and naming") {
    auto state = std::make_shared<TestStateA>();
    CHECK(state->name() == "StateA");
}

TEST_CASE("State: Lifecycle callbacks") {
    auto state = std::make_shared<TestStateA>();
    tree::Blackboard bb;

    state->onEnter(bb);
    CHECK(bb.get<int>("enter_count").value() == 1);

    state->onUpdate(bb);
    CHECK(bb.get<int>("update_count").value() == 1);

    state->onExit(bb);
    CHECK(bb.get<int>("exit_count").value() == 1);
}

TEST_CASE("State: Custom callbacks") {
    auto state = std::make_shared<state::State>("CustomState");
    tree::Blackboard bb;

    bool entered = false;
    bool updated = false;
    bool exited = false;

    state->setOnEnter([&entered](tree::Blackboard &) { entered = true; });
    state->setOnUpdate([&updated](tree::Blackboard &) { updated = true; });
    state->setOnExit([&exited](tree::Blackboard &) { exited = true; });

    state->onEnter(bb);
    CHECK(entered);

    state->onUpdate(bb);
    CHECK(updated);

    state->onExit(bb);
    CHECK(exited);
}

TEST_CASE("Transition: Basic transition with condition") {
    auto stateA = std::make_shared<TestStateA>();
    auto stateB = std::make_shared<TestStateB>();
    tree::Blackboard bb;

    auto transition = std::make_shared<state::Transition>(
        stateA, stateB, [](tree::Blackboard &bb) { return bb.get<bool>("should_transition").value_or(false); });

    SUBCASE("Transition should not occur when condition is false") {
        bb.set<bool>("should_transition", false);
        CHECK_FALSE(transition->shouldTransition(bb));
    }

    SUBCASE("Transition should occur when condition is true") {
        bb.set<bool>("should_transition", true);
        CHECK(transition->shouldTransition(bb));
    }

    SUBCASE("Transition stores correct from/to states") {
        CHECK(transition->from() == stateA);
        CHECK(transition->to() == stateB);
    }
}
