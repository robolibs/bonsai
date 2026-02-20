#include "doctest/doctest.h"

#include "stateup/state/builder.hpp"
#include <string>

using namespace stateup;

TEST_CASE("CompositeState: orthogonal regions tick independently") {
    // Build composite with two regions A and B
    auto machine = state::Builder()
                       .compositeState("Root", state::CompositeState::HistoryType::None,
                                       [&](state::Builder &b) {
                                           b.state("Idle");
                                           b.initial("Idle");
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

    // First tick enters Root and both regions; both regions transition once
    machine->tick();
    CHECK(machine->getCurrentStateName() == std::string("Root"));

    // Regions should have transitioned independently to their next states
    // We access via the CompositeState API
    auto root = std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState());
    REQUIRE(root);

    CHECK(root->getRegionCurrentState("RegionA") == std::string("A2"));
    CHECK(root->getRegionCurrentState("RegionB") == std::string("B2"));

    // Another tick should keep them in the same states (no further transitions defined)
    machine->tick();
    CHECK(root->getRegionCurrentState("RegionA") == std::string("A2"));
    CHECK(root->getRegionCurrentState("RegionB") == std::string("B2"));
}
