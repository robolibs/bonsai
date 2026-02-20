#include <stateup/stateup.hpp>
#include <chrono>
#include <iostream>
#include <thread>

using namespace stateup::tree;

// Example: Smart Home Automation System
// Demonstrates all the new control flow nodes in a realistic scenario

class SmartHomeDemo {
  public:
    void run() {
        std::cout << "\n=== Smart Home Automation System Demo ===\n" << std::endl;
        std::cout << "Demonstrating new control flow nodes:\n";
        std::cout << "- ConditionalNode (if/else)\n";
        std::cout << "- WhileNode (loops)\n";
        std::cout << "- ForNode (fixed iterations)\n";
        std::cout << "- SwitchNode (multi-way branching)\n";
        std::cout << "- MemoryNode (state retention)\n";
        std::cout << "- ConditionalSequence\n" << std::endl;

        // Build the smart home behavior tree
        auto tree = buildSmartHomeTree();

        // Simulate different scenarios
        simulateMorningRoutine(tree);
        simulateSecurityAlert(tree);
        simulateEnergyOptimization(tree);
    }

  private:
    Tree buildSmartHomeTree() {
        return Builder()
            .selector() // Main selector for different modes

            // Security mode check
            .sequence()
            .action([](Blackboard &bb) {
                return bb.get<bool>("security_alert").value_or(false) ? Status::Success : Status::Failure;
            })
            .action([](Blackboard &) {
                std::cout << "🚨 SECURITY ALERT DETECTED!" << std::endl;
                return Status::Success;
            })
            // Use switch node for different alert types
            .switchNode([](Blackboard &bb) { return bb.get<std::string>("alert_type").value_or("unknown"); })
            .addCase("motion",
                     [](Builder &b) {
                         b.sequence()
                             .action([](Blackboard &) {
                                 std::cout << "  📹 Activating cameras..." << std::endl;
                                 return Status::Success;
                             })
                             .action([](Blackboard &) {
                                 std::cout << "  💡 Turning on all lights..." << std::endl;
                                 return Status::Success;
                             })
                             .action([](Blackboard &) {
                                 std::cout << "  📱 Sending notification to owner..." << std::endl;
                                 return Status::Success;
                             })
                             .end();
                     })
            .addCase("fire",
                     [](Builder &b) {
                         b.sequence()
                             .action([](Blackboard &) {
                                 std::cout << "  🚒 Calling fire department..." << std::endl;
                                 return Status::Success;
                             })
                             .action([](Blackboard &) {
                                 std::cout << "  💨 Activating ventilation system..." << std::endl;
                                 return Status::Success;
                             })
                             .action([](Blackboard &) {
                                 std::cout << "  🚪 Unlocking all doors for evacuation..." << std::endl;
                                 return Status::Success;
                             })
                             .end();
                     })
            .addCase("intrusion",
                     [](Builder &b) {
                         b.sequence()
                             .action([](Blackboard &) {
                                 std::cout << "  🔒 Locking all doors..." << std::endl;
                                 return Status::Success;
                             })
                             .action([](Blackboard &) {
                                 std::cout << "  🚔 Calling police..." << std::endl;
                                 return Status::Success;
                             })
                             .action([](Blackboard &) {
                                 std::cout << "  🔊 Activating alarm..." << std::endl;
                                 return Status::Success;
                             })
                             .end();
                     })
            .defaultCase([](Builder &b) {
                b.action([](Blackboard &) {
                    std::cout << "  ⚠️ Unknown alert type!" << std::endl;
                    return Status::Success;
                });
            })
            .end()

            // Morning routine using conditional and loops
            .sequence()
            .action([](Blackboard &bb) {
                return bb.get<std::string>("mode").value_or("") == "morning" ? Status::Success : Status::Failure;
            })
            .action([](Blackboard &) {
                std::cout << "\n☀️ Starting Morning Routine..." << std::endl;
                return Status::Success;
            })

            // Conditional: Check if it's a weekday
            .condition([](Blackboard &bb) { return bb.get<bool>("is_weekday").value_or(false); },
                       [](Builder &b) {
                           // Weekday routine
                           b.sequence()
                               .action([](Blackboard &) {
                                   std::cout << "  📅 Weekday detected" << std::endl;
                                   return Status::Success;
                               })
                               .action([](Blackboard &) {
                                   std::cout << "  ☕ Starting coffee maker..." << std::endl;
                                   return Status::Success;
                               })
                               .action([](Blackboard &) {
                                   std::cout << "  📰 Displaying news on smart display..." << std::endl;
                                   return Status::Success;
                               })
                               .end();
                       },
                       [](Builder &b) {
                           // Weekend routine
                           b.sequence()
                               .action([](Blackboard &) {
                                   std::cout << "  🌴 Weekend detected" << std::endl;
                                   return Status::Success;
                               })
                               .action([](Blackboard &) {
                                   std::cout << "  🎵 Playing relaxing music..." << std::endl;
                                   return Status::Success;
                               })
                               .end();
                       })

            // Gradually increase lighting using while loop
            .whileLoop([](Blackboard &bb) { return bb.get<int>("light_level").value_or(0) < 100; },
                       [](Builder &b) {
                           b.action([](Blackboard &bb) {
                               int level = bb.get<int>("light_level").value_or(0);
                               level += 20;
                               bb.set("light_level", level);
                               std::cout << "  💡 Lights at " << level << "%" << std::endl;
                               std::this_thread::sleep_for(std::chrono::milliseconds(100));
                               return Status::Success;
                           });
                       })

            // Check each room using for loop
            .forLoop([](Blackboard &bb) { return bb.get<int>("num_rooms").value_or(3); },
                     [](Builder &b) {
                         b.action([](Blackboard &bb) {
                             static int room = 0;
                             room++;
                             std::cout << "  🏠 Checking room " << room << "..." << std::endl;
                             if (room >= bb.get<int>("num_rooms").value_or(3)) {
                                 room = 0; // Reset for next run
                             }
                             return Status::Success;
                         });
                     })
            .end()

            // Energy optimization mode
            .sequence()
            .action([](Blackboard &bb) {
                return bb.get<std::string>("mode").value_or("") == "energy_save" ? Status::Success : Status::Failure;
            })
            .action([](Blackboard &) {
                std::cout << "\n⚡ Energy Optimization Mode..." << std::endl;
                return Status::Success;
            })

            // Use conditional sequence to check conditions before each action
            .conditionalSequence()
            // Turn off lights if nobody home
            .action([](Blackboard &bb) {
                if (!bb.get<bool>("anyone_home").value_or(true)) {
                    std::cout << "  🏠 Nobody home - turning off all lights" << std::endl;
                }
                return Status::Success;
            })
            // Adjust thermostat if temperature ok
            .action([](Blackboard &bb) {
                int temp = bb.get<int>("temperature").value_or(20);
                if (temp > 18 && temp < 24) {
                    std::cout << "  🌡️ Temperature optimal (" << temp << "°C) - reducing HVAC" << std::endl;
                }
                return Status::Success;
            })
            // Solar panel check
            .action([](Blackboard &bb) {
                if (bb.get<bool>("solar_available").value_or(false)) {
                    std::cout << "  ☀️ Switching to solar power" << std::endl;
                }
                return Status::Success;
            })
            .end()
            .end()

            // Default idle behavior
            .action([](Blackboard &) {
                std::cout << "💤 System idle - monitoring..." << std::endl;
                return Status::Success;
            })

            .end()
            .build();
    }

    void simulateMorningRoutine(Tree &tree) {
        std::cout << "\n--- Scenario 1: Morning Routine (Weekday) ---" << std::endl;
        tree.blackboard().set("mode", std::string("morning"));
        tree.blackboard().set("is_weekday", true);
        tree.blackboard().set("light_level", 0);
        tree.blackboard().set("num_rooms", 3);
        tree.tick();
        tree.reset();

        std::cout << "\n--- Scenario 2: Morning Routine (Weekend) ---" << std::endl;
        tree.blackboard().set("mode", std::string("morning"));
        tree.blackboard().set("is_weekday", false);
        tree.blackboard().set("light_level", 0);
        tree.tick();
        tree.reset();
    }

    void simulateSecurityAlert(Tree &tree) {
        std::cout << "\n--- Scenario 3: Security Alert (Motion) ---" << std::endl;
        tree.blackboard().set("security_alert", true);
        tree.blackboard().set("alert_type", std::string("motion"));
        tree.blackboard().set("mode", std::string(""));
        tree.tick();
        tree.reset();
        tree.blackboard().set("security_alert", false);

        std::cout << "\n--- Scenario 4: Security Alert (Fire) ---" << std::endl;
        tree.blackboard().set("security_alert", true);
        tree.blackboard().set("alert_type", std::string("fire"));
        tree.tick();
        tree.reset();
        tree.blackboard().set("security_alert", false);
    }

    void simulateEnergyOptimization(Tree &tree) {
        std::cout << "\n--- Scenario 5: Energy Optimization ---" << std::endl;
        tree.blackboard().set("mode", std::string("energy_save"));
        tree.blackboard().set("anyone_home", false);
        tree.blackboard().set("temperature", 21);
        tree.blackboard().set("solar_available", true);
        tree.tick();
        tree.reset();
    }
};

// Example 2: Game AI using Memory Nodes
class GameAIDemo {
  public:
    void run() {
        std::cout << "\n\n=== Game AI with Memory Nodes Demo ===\n" << std::endl;

        auto enemyAI = Builder()
                           .selector()
                           // Remember if we found the player recently
                           .sequence()
                           .memory(MemoryNode::MemoryPolicy::REMEMBER_SUCCESSFUL)
                           .action([this](Blackboard &bb) {
                               if (!playerSpotted_) {
                                   std::cout << "👁️ Searching for player..." << std::endl;
                                   searchAttempts_++;
                                   if (searchAttempts_ >= 3) {
                                       playerSpotted_ = true;
                                       std::cout << "🎯 Player found!" << std::endl;
                                       return Status::Success;
                                   }
                                   return Status::Failure;
                               }
                               return Status::Success;
                           })
                           .action([](Blackboard &) {
                               std::cout << "⚔️ Attacking player!" << std::endl;
                               return Status::Success;
                           })
                           .end()

                           // Patrol behavior
                           .action([](Blackboard &) {
                               std::cout << "🚶 Patrolling area..." << std::endl;
                               return Status::Success;
                           })
                           .end()
                           .build();

        std::cout << "Enemy AI behavior over multiple ticks:" << std::endl;
        std::cout << "(Memory node remembers if player was found)\n" << std::endl;

        for (int i = 0; i < 5; ++i) {
            std::cout << "Tick " << (i + 1) << ": ";
            enemyAI.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

  private:
    bool playerSpotted_ = false;
    int searchAttempts_ = 0;
};

int main() {
    try {
        SmartHomeDemo smartHome;
        smartHome.run();

        GameAIDemo gameAI;
        gameAI.run();

        std::cout << "\n✅ Control flow demo completed successfully!\n" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}