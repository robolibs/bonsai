#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "bonsai/state/builder.hpp"
#include <algorithm>
#include <string>
#include <vector>

using namespace bonsai;

TEST_CASE("CompositeState: getRegionNames returns expected names") {
    auto machine = state::Builder()
                       .compositeState("Root", state::CompositeState::HistoryType::None,
                                       [&](state::Builder &b) {
                                           b.state("Idle");
                                           b.initial("Idle");
                                       })
                       .region("RegionA",
                               [](state::Builder &r) {
                                   r.state("A1");
                                   r.initial("A1");
                               })
                       .region("RegionB",
                               [](state::Builder &r) {
                                   r.state("B1");
                                   r.initial("B1");
                               })
                       .initial("Root")
                       .build();

    machine->tick();
    auto root = std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState());
    REQUIRE(root);

    auto names = root->getRegionNames();
    std::sort(names.begin(), names.end());

    CHECK(names.size() == 2);
    CHECK(names[0] == std::string("RegionA"));
    CHECK(names[1] == std::string("RegionB"));
}

TEST_CASE("CompositeState: region BB is isolated from parent BB") {
    // Parent sets a flag, but region transitions only if its own BB has it
    auto machine = state::Builder()
                       .compositeState("Root", state::CompositeState::HistoryType::None,
                                       [&](state::Builder &b) {
                                           b.state("Idle");
                                           b.initial("Idle");
                                       })
                       .region("ParentFlagRegion",
                               [](state::Builder &r) {
                                   r.state("P1");
                                   r.state("P2");
                                   r.state("P1").transitionTo(
                                       "P2", [](tree::Blackboard &bb) { return bb.get<bool>("go").value_or(false); });
                                   r.initial("P1");
                               })
                       .region("SelfFlagRegion",
                               [](state::Builder &r) {
                                   r.state("S1").onEnter([](tree::Blackboard &bb) { bb.set<bool>("go", true); });
                                   r.state("S2");
                                   r.state("S1").transitionTo(
                                       "S2", [](tree::Blackboard &bb) { return bb.get<bool>("go").value_or(false); });
                                   r.initial("S1");
                               })
                       .initial("Root")
                       .build();

    // Set flag on parent blackboard only
    machine->blackboard().set<bool>("go", true);

    machine->tick();
    auto root = std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState());
    REQUIRE(root);

    // ParentFlagRegion should NOT transition (flag is not in region BB)
    CHECK(root->getRegionCurrentState("ParentFlagRegion") == std::string("P1"));
    // SelfFlagRegion should transition due to its own onEnter setting
    CHECK(root->getRegionCurrentState("SelfFlagRegion") == std::string("S2"));
}

TEST_CASE("CompositeState: reset re-initializes regions") {
    // Region transitions after two updates based on its own counter
    auto machine = state::Builder()
                       .compositeState("Root", state::CompositeState::HistoryType::None,
                                       [&](state::Builder &b) {
                                           b.state("Idle");
                                           b.initial("Idle");
                                       })
                       .region("CounterRegion",
                               [](state::Builder &r) {
                                   r.state("C1")
                                       .onEnter([](tree::Blackboard &bb) { bb.set<int>("count", 0); })
                                       .onUpdate([](tree::Blackboard &bb) {
                                           int c = bb.get<int>("count").value_or(0);
                                           bb.set<int>("count", c + 1);
                                       });
                                   r.state("C2");
                                   r.state("C1").transitionTo("C2", [](tree::Blackboard &bb) {
                                       return bb.get<int>("count").value_or(0) >= 2;
                                   });
                                   r.initial("C1");
                               })
                       .initial("Root")
                       .build();

    // First tick: enter Root and region; region count becomes 1 (no transition yet)
    machine->tick();
    auto root = std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState());
    REQUIRE(root);
    CHECK(root->getRegionCurrentState("CounterRegion") == std::string("C1"));

    // Second tick: region updates to count=2 and transitions to C2
    machine->tick();
    CHECK(root->getRegionCurrentState("CounterRegion") == std::string("C2"));

    // Reset entire machine: regions should reinitialize to their initial states
    machine->reset();
    CHECK(machine->getCurrentStateName() == std::string("Root"));
    // After reset, Root.onEnter performs one region tick; still should be at C1
    CHECK(std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState())
              ->getRegionCurrentState("CounterRegion") == std::string("C1"));
}

TEST_CASE("CompositeState: nested machine and regions update together") {
    auto machine =
        state::Builder()
            .compositeState("Root", state::CompositeState::HistoryType::None,
                            [&](state::Builder &nested) {
                                nested.state("N1");
                                nested.state("N2");
                                nested.state("N1").transitionTo("N2", [](tree::Blackboard &) { return true; });
                                nested.initial("N1");
                            })
            .region("RegionA",
                    [](state::Builder &r) {
                        r.state("A1");
                        r.state("A2");
                        r.state("A1").transitionTo("A2", [](tree::Blackboard &) { return true; });
                        r.initial("A1");
                    })
            .region("RegionB",
                    [](state::Builder &r) {
                        r.state("B1");
                        r.state("B2");
                        r.state("B1").transitionTo("B2", [](tree::Blackboard &) { return true; });
                        r.initial("B1");
                    })
            .initial("Root")
            .build();

    machine->tick();
    auto root = std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState());
    REQUIRE(root);

    // Nested machine should have transitioned to N2
    CHECK(root->getCurrentSubstate() == std::string("N2"));
    // Both regions should have transitioned
    CHECK(root->getRegionCurrentState("RegionA") == std::string("A2"));
    CHECK(root->getRegionCurrentState("RegionB") == std::string("B2"));

    // Subsequent tick should keep them stable (no further transitions)
    machine->tick();
    CHECK(root->getCurrentSubstate() == std::string("N2"));
    CHECK(root->getRegionCurrentState("RegionA") == std::string("A2"));
    CHECK(root->getRegionCurrentState("RegionB") == std::string("B2"));
}
