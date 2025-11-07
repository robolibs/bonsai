#include <bonsai/bonsai.hpp>
#include <iostream>
#include <memory>

using namespace bonsai;

// Example 1: Simple traffic light state machine
void trafficLightExample() {
    std::cout << "\n=== Traffic Light State Machine ===" << std::endl;

    auto machine =
        state::Builder()
            .initial("red")
            .state("red")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🔴 RED - Stop!" << std::endl;
                bb.set("timer", 0);
            })
            .onUpdate([](tree::Blackboard &bb) {
                auto timer = bb.get<int>("timer").value_or(0);
                bb.set("timer", timer + 1);
            })
            .transitionTo("green", [](tree::Blackboard &bb) { return bb.get<int>("timer").value_or(0) >= 3; })
            .state("green")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🟢 GREEN - Go!" << std::endl;
                bb.set("timer", 0);
            })
            .onUpdate([](tree::Blackboard &bb) {
                auto timer = bb.get<int>("timer").value_or(0);
                bb.set("timer", timer + 1);
            })
            .transitionTo("yellow", [](tree::Blackboard &bb) { return bb.get<int>("timer").value_or(0) >= 5; })
            .state("yellow")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🟡 YELLOW - Caution!" << std::endl;
                bb.set("timer", 0);
            })
            .onUpdate([](tree::Blackboard &bb) {
                auto timer = bb.get<int>("timer").value_or(0);
                bb.set("timer", timer + 1);
            })
            .transitionTo("red", [](tree::Blackboard &bb) { return bb.get<int>("timer").value_or(0) >= 2; })
            .build();

    // Run the traffic light for 15 ticks
    for (int i = 0; i < 15; ++i) {
        machine->tick();
    }
}

// Example 2: Character state machine (Idle, Walk, Run, Jump)
void characterStateExample() {
    std::cout << "\n=== Character State Machine ===" << std::endl;

    auto machine =
        state::Builder()
            .initial("idle")
            .state("idle")
            .onEnter([](tree::Blackboard &bb) { std::cout << "🧍 Character is IDLE" << std::endl; })
            .onUpdate([](tree::Blackboard &bb) {
                // Simulate player input
                static int tick = 0;
                tick++;
                if (tick == 2) {
                    bb.set("input_walk", true);
                } else if (tick == 5) {
                    bb.set("input_run", true);
                } else if (tick == 8) {
                    bb.set("input_jump", true);
                }
            })
            .transitionTo("walk", [](tree::Blackboard &bb) { return bb.get<bool>("input_walk").value_or(false); })
            .state("walk")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🚶 Character is WALKING" << std::endl;
                bb.remove("input_walk");
            })
            .transitionTo("run", [](tree::Blackboard &bb) { return bb.get<bool>("input_run").value_or(false); })
            .transitionTo("idle", [](tree::Blackboard &bb) { return bb.get<bool>("input_stop").value_or(false); })
            .state("run")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🏃 Character is RUNNING" << std::endl;
                bb.remove("input_run");
            })
            .transitionTo("jump", [](tree::Blackboard &bb) { return bb.get<bool>("input_jump").value_or(false); })
            .transitionTo("walk", [](tree::Blackboard &bb) { return bb.get<bool>("input_walk_only").value_or(false); })
            .state("jump")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🦘 Character is JUMPING" << std::endl;
                bb.set("jump_timer", 0);
                bb.remove("input_jump");
            })
            .onUpdate([](tree::Blackboard &bb) {
                auto timer = bb.get<int>("jump_timer").value_or(0);
                bb.set("jump_timer", timer + 1);
            })
            .onExit([](tree::Blackboard &bb) { std::cout << "  Landing..." << std::endl; })
            .transitionTo("idle", [](tree::Blackboard &bb) { return bb.get<int>("jump_timer").value_or(0) >= 3; })
            .build();

    // Run the character state machine
    for (int i = 0; i < 12; ++i) {
        std::cout << "Tick " << i << ": ";
        machine->tick();
    }
}

// Example 3: Enemy AI state machine
void enemyAIExample() {
    std::cout << "\n=== Enemy AI State Machine ===" << std::endl;

    auto machine = state::Builder()
                       .initial("patrol")
                       .state("patrol")
                       .onEnter([](tree::Blackboard &bb) {
                           std::cout << "👁️  Enemy PATROLLING" << std::endl;
                           bb.set("patrol_points", 0);
                       })
                       .onUpdate([](tree::Blackboard &bb) {
                           auto points = bb.get<int>("patrol_points").value_or(0);
                           bb.set("patrol_points", points + 1);
                           // Simulate player detection
                           if (points == 3) {
                               bb.set("player_distance", 8.0f);
                           }
                       })
                       .transitionTo("chase",
                                     [](tree::Blackboard &bb) {
                                         auto dist = bb.get<float>("player_distance").value_or(100.0f);
                                         return dist < 10.0f;
                                     })
                       .state("chase")
                       .onEnter([](tree::Blackboard &bb) { std::cout << "🏃 Enemy CHASING player!" << std::endl; })
                       .onUpdate([](tree::Blackboard &bb) {
                           auto dist = bb.get<float>("player_distance").value_or(10.0f);
                           bb.set("player_distance", dist - 1.5f); // Move closer
                       })
                       .transitionTo("attack",
                                     [](tree::Blackboard &bb) {
                                         auto dist = bb.get<float>("player_distance").value_or(10.0f);
                                         return dist <= 2.0f;
                                     })
                       .transitionTo("patrol",
                                     [](tree::Blackboard &bb) {
                                         auto dist = bb.get<float>("player_distance").value_or(0.0f);
                                         return dist > 15.0f;
                                     })
                       .state("attack")
                       .onEnter([](tree::Blackboard &bb) {
                           std::cout << "⚔️  Enemy ATTACKING!" << std::endl;
                           bb.set("attack_cooldown", 0);
                       })
                       .onUpdate([](tree::Blackboard &bb) {
                           auto cooldown = bb.get<int>("attack_cooldown").value_or(0);
                           bb.set("attack_cooldown", cooldown + 1);
                           // Player runs away
                           if (cooldown > 2) {
                               auto dist = bb.get<float>("player_distance").value_or(2.0f);
                               bb.set("player_distance", dist + 5.0f);
                           }
                       })
                       .transitionTo("chase",
                                     [](tree::Blackboard &bb) {
                                         auto dist = bb.get<float>("player_distance").value_or(0.0f);
                                         return dist > 2.0f && dist <= 15.0f;
                                     })
                       .build();

    // Run the enemy AI
    for (int i = 0; i < 15; ++i) {
        std::cout << "Tick " << i << " [";
        auto dist = machine->blackboard().get<float>("player_distance").value_or(100.0f);
        std::cout << "Distance: " << dist << "]: ";
        machine->tick();
    }
}

int main() {
    std::cout << "Bonsai State Machine Examples" << std::endl;
    std::cout << "==============================" << std::endl;

    trafficLightExample();
    characterStateExample();
    enemyAIExample();

    std::cout << "\n✅ All examples completed!" << std::endl;
    return 0;
}
