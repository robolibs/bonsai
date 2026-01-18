#include <bonsai/bonsai.hpp>
#include <chrono>
#include <doctest/doctest.h>
#include <thread>

using namespace bonsai::tree;

// Helper class to simulate a robot for testing
class TestRobot {
  public:
    double battery = 100.0;
    double position = 0.0;
    double target_position = 10.0;
    bool is_charging = false;
    bool has_obstacle = false;

    void update() {
        if (!is_charging) {
            battery -= 0.5;
        } else {
            battery = std::min(100.0, battery + 2.0);
        }
    }

    bool is_at_target() const { return std::abs(position - target_position) < 0.1; }

    bool needs_charging() const { return battery < 20.0; }
};

TEST_CASE("Integration - Robot behavior tree") {
    TestRobot robot;

    SUBCASE("Simple move to target") {
        auto tree = Builder()
                        .sequence()
                        .action([&robot](Blackboard &bb) -> Status {
                            if (robot.is_at_target()) {
                                return Status::Success;
                            }

                            // Simulate movement
                            double direction = (robot.target_position > robot.position) ? 1.0 : -1.0;
                            robot.position += direction * 0.5;
                            robot.update();

                            return robot.is_at_target() ? Status::Success : Status::Running;
                        })
                        .end()
                        .build();

        Status result = Status::Running;
        int max_iterations = 50;
        int iterations = 0;

        while (result == Status::Running && iterations < max_iterations) {
            result = tree.tick();
            iterations++;
        }

        CHECK(result == Status::Success);
        CHECK(robot.is_at_target());
        CHECK(iterations > 0);
        CHECK(iterations < max_iterations);
    }

    SUBCASE("Battery management") {
        robot.battery = 15.0; // Low battery

        auto tree = Builder()
                        .selector()
                        // Check if charging is needed
                        .sequence()
                        .action([&robot](Blackboard &bb) -> Status {
                            return robot.needs_charging() ? Status::Success : Status::Failure;
                        })
                        .action([&robot](Blackboard &bb) -> Status {
                            robot.is_charging = true;
                            robot.update();

                            // Charge until battery is full
                            return (robot.battery >= 100.0) ? Status::Success : Status::Running;
                        })
                        .end()
                        // Normal operations
                        .action([&robot](Blackboard &bb) -> Status {
                            robot.is_charging = false;
                            // Simulate other operations
                            robot.update();
                            return Status::Success;
                        })
                        .end()
                        .build();

        // Should start charging
        CHECK(tree.tick() == Status::Running);
        CHECK(robot.is_charging);
        CHECK(robot.battery > 15.0);

        // Continue charging
        Status result = Status::Running;
        int iterations = 0;
        while (result == Status::Running && iterations < 100) {
            result = tree.tick();
            iterations++;
        }

        CHECK(result == Status::Success);
        CHECK(robot.battery >= 100.0);
    }
}

TEST_CASE("Integration - Complex decision tree") {
    auto tree = Builder()
                    .selector()
                    // Emergency: Health too low
                    .sequence()
                    .action([](Blackboard &bb) -> Status {
                        auto health = bb.get<int>("health").value_or(100);
                        return (health < 30) ? Status::Success : Status::Failure;
                    })
                    .action([](Blackboard &bb) -> Status {
                        bb.set("action", std::string("retreat"));
                        return Status::Success;
                    })
                    .end()
                    // Combat: Enemy nearby and have ammo
                    .sequence()
                    .action([](Blackboard &bb) -> Status {
                        auto distance = bb.get<double>("enemy_distance").value_or(100.0);
                        auto ammo = bb.get<int>("ammo").value_or(0);
                        return (distance < 20.0 && ammo > 0) ? Status::Success : Status::Failure;
                    })
                    .action([](Blackboard &bb) -> Status {
                        bb.set("action", std::string("attack"));
                        auto ammo = bb.get<int>("ammo").value_or(0);
                        bb.set("ammo", ammo - 1);
                        return Status::Success;
                    })
                    .end()
                    // Default: Patrol
                    .action([](Blackboard &bb) -> Status {
                        bb.set("action", std::string("patrol"));
                        return Status::Success;
                    })
                    .end()
                    .build();

    SUBCASE("Should attack when enemy is close") {
        tree.blackboard().set("health", 80);
        tree.blackboard().set("ammo", 5);
        tree.blackboard().set("enemy_distance", 15.0);

        CHECK(tree.tick() == Status::Success);
        auto action = tree.blackboard().get<std::string>("action");
        REQUIRE(action.has_value());
        CHECK(action.value() == "attack");

        auto remaining_ammo = tree.blackboard().get<int>("ammo");
        REQUIRE(remaining_ammo.has_value());
        CHECK(remaining_ammo.value() == 4);
    }

    SUBCASE("Should retreat when health is low") {
        tree.blackboard().set("health", 20);
        tree.blackboard().set("ammo", 5);
        tree.blackboard().set("enemy_distance", 15.0);

        CHECK(tree.tick() == Status::Success);
        auto action = tree.blackboard().get<std::string>("action");
        REQUIRE(action.has_value());
        CHECK(action.value() == "retreat");
    }

    SUBCASE("Should patrol when no threats") {
        tree.blackboard().set("health", 80);
        tree.blackboard().set("enemy_distance", 50.0);
        tree.blackboard().set("ammo", 0);

        CHECK(tree.tick() == Status::Success);
        auto action = tree.blackboard().get<std::string>("action");
        REQUIRE(action.has_value());
        CHECK(action.value() == "patrol");
    }
}

TEST_CASE("Integration - State persistence across ticks") {
    auto tree = Builder()
                    .sequence()
                    .action([](Blackboard &bb) -> Status {
                        auto counter = bb.get<int>("counter").value_or(0);
                        bb.set("counter", counter + 1);

                        // Only succeed after 3 ticks
                        return (counter >= 2) ? Status::Success : Status::Running;
                    })
                    .action([](Blackboard &bb) -> Status {
                        bb.set("completed", true);
                        return Status::Success;
                    })
                    .end()
                    .build();

    // First tick - should be running
    CHECK(tree.tick() == Status::Running);
    CHECK(tree.blackboard().get<int>("counter").value() == 1);
    CHECK_FALSE(tree.blackboard().has("completed"));

    // Second tick - still running
    CHECK(tree.tick() == Status::Running);
    CHECK(tree.blackboard().get<int>("counter").value() == 2);
    CHECK_FALSE(tree.blackboard().has("completed"));

    // Third tick - should complete
    CHECK(tree.tick() == Status::Success);
    CHECK(tree.blackboard().get<int>("counter").value() == 3);
    CHECK(tree.blackboard().get<bool>("completed").value_or(false));
}

TEST_CASE("Integration - Tree reset and reexecution") {
    int global_counter = 0;

    auto tree = Builder()
                    .action([&global_counter](Blackboard &bb) -> Status {
                        global_counter++;
                        auto local_counter = bb.get<int>("local_counter").value_or(0);
                        bb.set("local_counter", local_counter + 1);

                        return (local_counter >= 2) ? Status::Success : Status::Running;
                    })
                    .build();

    // Execute until completion
    Status result = Status::Running;
    while (result == Status::Running) {
        result = tree.tick();
    }

    CHECK(result == Status::Success);
    CHECK(global_counter == 3);
    CHECK(tree.blackboard().get<int>("local_counter").value() == 3);

    // Reset tree (only resets nodes, not blackboard)
    tree.reset();

    // Execute again - blackboard persists, so it should complete faster
    result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(global_counter == 4); // Only one more execution since local_counter is already >= 2
    CHECK(tree.blackboard().get<int>("local_counter").value() == 4); // Blackboard persists
}
