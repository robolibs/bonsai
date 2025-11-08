#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "bonsai/state/builder.hpp"
#include <string>

using namespace bonsai;

TEST_CASE("Guard: Basic guard condition prevents transition") {
    auto machine = state::Builder()
                       .initial("idle")
                       .state("idle")
                       .onEnter([](tree::Blackboard &bb) { bb.set<int>("attempts", 0); })
                       .transitionTo("locked",
                                     [](tree::Blackboard &bb) {
                                         // Always try to transition
                                         return true;
                                     })
                       .state("locked")
                       .onGuard([](tree::Blackboard &bb) {
                           // Only allow transition if attempts < 3
                           return bb.get<int>("attempts").value_or(0) < 3;
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("status", "locked"); })
                       .build();

    machine->tick(); // Enter idle
    CHECK(machine->getCurrentStateName() == "idle");

    // First attempt - guard passes (attempts = 0 < 3)
    machine->tick(); // Try to transition to locked
    CHECK(machine->getCurrentStateName() == "locked");
    CHECK(machine->blackboard().get<std::string>("status").value() == "locked");
}

TEST_CASE("Guard: Guard condition blocks transition when false") {
    auto machine = state::Builder()
                       .initial("start")
                       .state("start")
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("can_proceed", false); })
                       .transitionTo("protected", [](tree::Blackboard &bb) { return true; })
                       .state("protected")
                       .onGuard([](tree::Blackboard &bb) {
                           // Only allow if can_proceed is true
                           return bb.get<bool>("can_proceed").value_or(false);
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("entered", "yes"); })
                       .build();

    machine->tick(); // Enter start (sets can_proceed = false)
    CHECK(machine->getCurrentStateName() == "start");

    machine->tick();                                   // Try to transition - guard should block
    CHECK(machine->getCurrentStateName() == "start");  // Still in start
    CHECK_FALSE(machine->blackboard().has("entered")); // Never entered protected
}

TEST_CASE("Guard: Guard allows transition when true") {
    auto machine = state::Builder()
                       .initial("start")
                       .state("start")
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("ready", true); })
                       .transitionTo("ready", [](tree::Blackboard &bb) { return true; })
                       .state("ready")
                       .onGuard([](tree::Blackboard &bb) { return bb.get<bool>("ready").value_or(false); })
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("status", "ready"); })
                       .build();

    machine->tick(); // Enter start (sets ready = true)
    machine->tick(); // Transition to ready - guard passes

    CHECK(machine->getCurrentStateName() == "ready");
    CHECK(machine->blackboard().get<std::string>("status").value() == "ready");
}

TEST_CASE("Guard: Password check example") {
    auto machine = state::Builder()
                       .initial("login")
                       .state("login")
                       .onUpdate([](tree::Blackboard &bb) {
                           // Simulate entering password
                           bb.set<std::string>("password", "secret123");
                       })
                       .transitionTo("authenticated",
                                     [](tree::Blackboard &bb) {
                                         return bb.has("password"); // Try to login when password is entered
                                     })
                       .state("authenticated")
                       .onGuard([](tree::Blackboard &bb) {
                           // Check if password is correct
                           auto password = bb.get<std::string>("password").value_or("");
                           return password == "secret123";
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("logged_in", true); })
                       .build();

    machine->tick(); // Enter login
    CHECK(machine->getCurrentStateName() == "login");

    machine->tick(); // Update (sets password), then try to transition
    CHECK(machine->getCurrentStateName() == "authenticated");
    CHECK(machine->blackboard().get<bool>("logged_in").value());
}

TEST_CASE("Guard: Failed password check example") {
    auto machine = state::Builder()
                       .initial("login")
                       .state("login")
                       .onUpdate([](tree::Blackboard &bb) {
                           // Simulate entering wrong password
                           bb.set<std::string>("password", "wrong");
                       })
                       .transitionTo("authenticated",
                                     [](tree::Blackboard &bb) {
                                         return bb.has("password"); // Try to login when password is entered
                                     })
                       .state("authenticated")
                       .onGuard([](tree::Blackboard &bb) {
                           // Check if password is correct
                           auto password = bb.get<std::string>("password").value_or("");
                           return password == "secret123";
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("logged_in", true); })
                       .build();

    machine->tick(); // Enter login
    CHECK(machine->getCurrentStateName() == "login");

    machine->tick();                                     // Update (sets wrong password), then try to transition
    CHECK(machine->getCurrentStateName() == "login");    // Guard blocks, stays in login
    CHECK_FALSE(machine->blackboard().has("logged_in")); // Never logged in
}

TEST_CASE("Guard: State without guard always allows transition") {
    auto machine = state::Builder()
                       .initial("a")
                       .state("a")
                       .transitionTo("b", [](tree::Blackboard &bb) { return true; })
                       .state("b") // No guard defined
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("state", "b"); })
                       .build();

    machine->tick(); // Enter a
    machine->tick(); // Transition to b (no guard, so always allowed)

    CHECK(machine->getCurrentStateName() == "b");
    CHECK(machine->blackboard().get<std::string>("state").value() == "b");
}

TEST_CASE("Guard: Complex guard with multiple conditions") {
    auto machine = state::Builder()
                       .initial("checking")
                       .state("checking")
                       .onEnter([](tree::Blackboard &bb) {
                           bb.set<int>("age", 25);
                           bb.set<bool>("has_license", true);
                           bb.set<int>("experience_years", 3);
                       })
                       .transitionTo("approved", [](tree::Blackboard &bb) { return true; })
                       .state("approved")
                       .onGuard([](tree::Blackboard &bb) {
                           // Complex guard: must be 18+, have license, and 2+ years experience
                           auto age = bb.get<int>("age").value_or(0);
                           auto has_license = bb.get<bool>("has_license").value_or(false);
                           auto experience = bb.get<int>("experience_years").value_or(0);
                           return age >= 18 && has_license && experience >= 2;
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("approved", true); })
                       .build();

    machine->tick(); // Enter checking
    machine->tick(); // Try to transition - guard checks all conditions

    CHECK(machine->getCurrentStateName() == "approved");
    CHECK(machine->blackboard().get<bool>("approved").value());
}

TEST_CASE("Guard: Failed complex guard") {
    auto machine = state::Builder()
                       .initial("checking")
                       .state("checking")
                       .onEnter([](tree::Blackboard &bb) {
                           bb.set<int>("age", 25);
                           bb.set<bool>("has_license", true);
                           bb.set<int>("experience_years", 1); // Too little experience!
                       })
                       .transitionTo("approved", [](tree::Blackboard &bb) { return true; })
                       .state("approved")
                       .onGuard([](tree::Blackboard &bb) {
                           auto age = bb.get<int>("age").value_or(0);
                           auto has_license = bb.get<bool>("has_license").value_or(false);
                           auto experience = bb.get<int>("experience_years").value_or(0);
                           return age >= 18 && has_license && experience >= 2;
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("approved", true); })
                       .build();

    machine->tick(); // Enter checking
    machine->tick(); // Try to transition - guard fails (experience < 2)

    CHECK(machine->getCurrentStateName() == "checking"); // Blocked by guard
    CHECK_FALSE(machine->blackboard().has("approved"));
}
