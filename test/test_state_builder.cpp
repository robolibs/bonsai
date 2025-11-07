#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "bonsai/state/builder.hpp"
#include <string>

using namespace bonsai;

TEST_CASE("Builder: Basic state machine construction") {
    auto machine = state::Builder()
                       .initial("idle")
                       .state("idle")
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("last_entered", "idle"); })
                       .build();

    CHECK(machine != nullptr);
    machine->tick();
    CHECK(machine->getCurrentStateName() == "idle");
    CHECK(machine->blackboard().get<std::string>("last_entered").value() == "idle");
}

TEST_CASE("Builder: State with all callbacks") {
    bool entered = false;
    bool updated = false;
    bool exited = false;

    auto machine = state::Builder()
                       .initial("test")
                       .state("test")
                       .onEnter([&entered](tree::Blackboard &) { entered = true; })
                       .onUpdate([&updated](tree::Blackboard &) { updated = true; })
                       .onExit([&exited](tree::Blackboard &) { exited = true; })
                       .build();

    machine->tick(); // Enter only
    CHECK(entered);
    CHECK_FALSE(updated);

    machine->tick(); // Update
    CHECK(updated);

    machine->reset();
    CHECK(exited);
}

TEST_CASE("Builder: Simple transition") {
    auto machine = state::Builder()
                       .initial("a")
                       .state("a")
                       .onEnter([](tree::Blackboard &bb) { bb.set<int>("counter", 0); })
                       .onUpdate([](tree::Blackboard &bb) {
                           int count = bb.get<int>("counter").value_or(0);
                           bb.set<int>("counter", count + 1);
                       })
                       .transitionTo("b", [](tree::Blackboard &bb) { return bb.get<int>("counter").value_or(0) >= 3; })
                       .state("b")
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("reached", "b"); })
                       .build();

    machine->tick(); // Enter a (counter=0)
    CHECK(machine->getCurrentStateName() == "a");

    machine->tick(); // Update a (counter=1)
    CHECK(machine->getCurrentStateName() == "a");

    machine->tick(); // Update a (counter=2)
    CHECK(machine->getCurrentStateName() == "a");

    machine->tick(); // Update a (counter=3), transition to b
    CHECK(machine->getCurrentStateName() == "b");
    CHECK(machine->blackboard().get<std::string>("reached").value() == "b");
}

TEST_CASE("Builder: Multiple states and transitions") {
    auto machine =
        state::Builder()
            .initial("idle")
            .state("idle")
            .onUpdate([](tree::Blackboard &bb) {
                int count = bb.get<int>("idle_count").value_or(0);
                bb.set<int>("idle_count", count + 1);
            })
            .transitionTo("walk", [](tree::Blackboard &bb) { return bb.get<int>("idle_count").value_or(0) >= 2; })
            .state("walk")
            .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("state", "walking"); })
            .onUpdate([](tree::Blackboard &bb) {
                int count = bb.get<int>("walk_count").value_or(0);
                bb.set<int>("walk_count", count + 1);
            })
            .transitionTo("run", [](tree::Blackboard &bb) { return bb.get<int>("walk_count").value_or(0) >= 3; })
            .state("run")
            .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("state", "running"); })
            .build();

    // Start in idle
    machine->tick(); // Enter idle
    CHECK(machine->getCurrentStateName() == "idle");

    // Transition to walk
    machine->tick(); // Update idle (idle_count=1)
    machine->tick(); // Update idle (idle_count=2), transition to walk
    CHECK(machine->getCurrentStateName() == "walk");
    CHECK(machine->blackboard().get<std::string>("state").value() == "walking");

    // Transition to run
    machine->tick(); // Update walk (walk_count=1)
    machine->tick(); // Update walk (walk_count=2)
    machine->tick(); // Update walk (walk_count=3), transition to run
    CHECK(machine->getCurrentStateName() == "run");
    CHECK(machine->blackboard().get<std::string>("state").value() == "running");
}

TEST_CASE("Builder: Error handling - no initial state") {
    CHECK_THROWS_WITH(state::Builder().state("test").build(),
                      "No initial state set. Use initial() to set the starting state.");
}

TEST_CASE("Builder: Error handling - callbacks without state") {
    CHECK_THROWS_WITH(state::Builder().onEnter([](tree::Blackboard &) {}),
                      "No current state set. Call state() before onEnter");

    CHECK_THROWS_WITH(state::Builder().onUpdate([](tree::Blackboard &) {}),
                      "No current state set. Call state() before onUpdate");

    CHECK_THROWS_WITH(state::Builder().onExit([](tree::Blackboard &) {}),
                      "No current state set. Call state() before onExit");
}

TEST_CASE("Builder: Error handling - transition without state") {
    CHECK_THROWS_WITH(state::Builder().transitionTo("b", [](tree::Blackboard &) { return true; }),
                      "No current state set. Call state() before transitionTo");
}

TEST_CASE("Builder: Auto-create target states") {
    // Target state "b" is created automatically by transitionTo
    auto machine =
        state::Builder().initial("a").state("a").transitionTo("b", [](tree::Blackboard &) { return true; }).build();

    machine->tick(); // Enter a
    machine->tick(); // Update a, transition to b (condition is always true)
    CHECK(machine->getCurrentStateName() == "b");
}

TEST_CASE("Builder: Multiple transitions from same state") {
    auto machine =
        state::Builder()
            .initial("start")
            .state("start")
            .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("choice", "none"); })
            .transitionTo("left",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("choice").value_or("") == "left"; })
            .transitionTo("right",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("choice").value_or("") == "right"; })
            .state("left")
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("went_left", true); })
            .state("right")
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("went_right", true); })
            .build();

    SUBCASE("Choose left") {
        machine->tick(); // Enter start (sets choice="none")
        machine->blackboard().set<std::string>("choice", "left");
        machine->tick(); // Update start, transition to left
        CHECK(machine->getCurrentStateName() == "left");
        CHECK(machine->blackboard().get<bool>("went_left").value());
    }

    SUBCASE("Choose right") {
        machine->tick(); // Enter start (sets choice="none")
        machine->blackboard().set<std::string>("choice", "right");
        machine->tick(); // Update start, transition to right
        CHECK(machine->getCurrentStateName() == "right");
        CHECK(machine->blackboard().get<bool>("went_right").value());
    }
}
