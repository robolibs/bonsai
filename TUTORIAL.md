# Bonsai Behavior Tree Tutorial

```
   ____                        _ 
  |  _ \                      (_)
  | |_) | ___  _ __  ___  __ _ _ 
  |  _ < / _ \| '_ \/ __|/ _` | |
  | |_) | (_) | | | \__ \ (_| | |
  |____/ \___/|_| |_|___/\__,_|_|
                                 
   Behavior Tree Library Tutorial
```

Welcome to the comprehensive Bonsai behavior tree tutorial! This guide will take you from beginner to advanced usage through practical examples and clear explanations.

## Table of Contents

1. [Basic Actions and Status](#lesson-1-basic-actions-and-status)
2. [The Blackboard - Shared Memory](#lesson-2-the-blackboard---shared-memory)
3. [Sequence Nodes - Do This, Then That](#lesson-3-sequence-nodes---do-this-then-that)
4. [Selector Nodes - Try This, Or That](#lesson-4-selector-nodes---try-this-or-that)
5. [Parallel Nodes - Do Multiple Things](#lesson-5-parallel-nodes---do-multiple-things)
6. [Decorators - Modify Behavior](#lesson-6-decorators---modify-behavior)
7. [Real-World Example - AI Guard](#lesson-7-real-world-example---ai-guard)
8. [Manual Node Creation](#lesson-8-manual-node-creation)
9. [Comprehensive Examples - All Features](#lesson-9-comprehensive-examples---all-features)

---

## Lesson 1: Basic Actions and Status

Actions are the building blocks of behavior trees. They return `Status::Success`, `Status::Failure`, or `Status::Running`.

```cpp
#include <bonsai/bonsai.hpp>
#include <iostream>

using namespace bonsai;

int main() {
    // Create simple actions with different outcomes
    auto always_succeed = [](Blackboard& bb) -> Status {
        std::cout << "✓ Action that always succeeds" << std::endl;
        return Status::Success;
    };
    
    auto always_fail = [](Blackboard& bb) -> Status {
        std::cout << "✗ Action that always fails" << std::endl;
        return Status::Failure;
    };
    
    auto running_action = [](Blackboard& bb) -> Status {
        // Get or initialize counter in blackboard
        auto counter_opt = bb.get<int>("action_counter");
        int counter = counter_opt.value_or(0);
        counter++;
        bb.set("action_counter", counter);
        
        std::cout << "⟳ Running action (step " << counter << "/3)" << std::endl;
        if (counter < 3) {
            return Status::Running;
        } else {
            bb.set("action_counter", 0); // Reset for next run
            std::cout << "✓ Running action completed!" << std::endl;
            return Status::Success;
        }
    };
    
    // Test individual actions
    Tree succeed_tree(std::make_unique<Action>(always_succeed));
    Tree fail_tree(std::make_unique<Action>(always_fail));
    Tree running_tree(std::make_unique<Action>(running_action));
    
    std::cout << "Testing individual actions:" << std::endl;
    std::cout << "Succeed result: " << (succeed_tree.tick() == Status::Success ? "SUCCESS" : "FAIL") << std::endl;
    std::cout << "Fail result: " << (fail_tree.tick() == Status::Success ? "SUCCESS" : "FAIL") << std::endl;
    
    // Running action needs multiple ticks
    std::cout << "Running action test:" << std::endl;
    Status running_result = Status::Running;
    while (running_result == Status::Running) {
        running_result = running_tree.tick();
    }
    std::cout << "Final result: " << (running_result == Status::Success ? "SUCCESS" : "FAIL") << std::endl;
    
    return 0;
}
```

**Key Concepts:**
- Actions are lambda functions that take a `Blackboard&` and return a `Status`
- `Status::Success` - The action completed successfully
- `Status::Failure` - The action failed
- `Status::Running` - The action is still in progress (needs more ticks)

---

## Lesson 2: The Blackboard - Shared Memory

The Blackboard is like a shared whiteboard where nodes can store and retrieve data to coordinate their behavior.

```cpp
auto blackboard_demo = Builder()
    .sequence()
        .action([](Blackboard& bb) -> Status {
            std::cout << "📝 Storing player data on blackboard..." << std::endl;
            bb.set("player_name", std::string("Alice"));
            bb.set("player_level", 42);
            bb.set("player_health", 85.5);
            bb.set("has_key", true);
            return Status::Success;
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "📖 Reading player data from blackboard..." << std::endl;
            
            auto name = bb.get<std::string>("player_name");
            auto level = bb.get<int>("player_level");
            auto health = bb.get<double>("player_health");
            auto has_key = bb.get<bool>("has_key");
            
            if (name.has_value()) {
                std::cout << "   Player: " << name.value() << std::endl;
            }
            if (level.has_value()) {
                std::cout << "   Level: " << level.value() << std::endl;
            }
            if (health.has_value()) {
                std::cout << "   Health: " << health.value() << "%" << std::endl;
            }
            if (has_key.has_value()) {
                std::cout << "   Has key: " << (has_key.value() ? "Yes" : "No") << std::endl;
            }
            
            return Status::Success;
        })
    .end()
    .build();

blackboard_demo.tick();
```

**Key Concepts:**
- The blackboard stores typed data by string keys
- Use `bb.set<T>(key, value)` to store data
- Use `bb.get<T>(key)` to retrieve data (returns `std::optional<T>`)
- The blackboard is thread-safe for concurrent access
- Data persists between tree ticks

---

## Lesson 3: Sequence Nodes - Do This, Then That

Sequences execute children in order. If any child fails, the sequence stops and returns failure.

```cpp
// Successful sequence
auto successful_sequence = Builder()
    .sequence()
        .action([](Blackboard& bb) -> Status {
            std::cout << "1️⃣ Check inventory" << std::endl;
            bb.set("has_materials", true);
            return Status::Success;
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "2️⃣ Craft item" << std::endl;
            auto has_materials = bb.get<bool>("has_materials");
            if (has_materials.value_or(false)) {
                bb.set("item_crafted", true);
                return Status::Success;
            }
            return Status::Failure;
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "3️⃣ Add to inventory" << std::endl;
            auto item_crafted = bb.get<bool>("item_crafted");
            if (item_crafted.value_or(false)) {
                std::cout << "✨ Item successfully crafted and stored!" << std::endl;
                return Status::Success;
            }
            return Status::Failure;
        })
    .end()
    .build();

std::cout << "Executing successful crafting sequence:" << std::endl;
Status craft_result = successful_sequence.tick();
std::cout << "Result: " << (craft_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;

// Failing sequence (stops at second step)
auto failing_sequence = Builder()
    .sequence()
        .action([](Blackboard& bb) -> Status {
            std::cout << "1️⃣ Attempt to open door" << std::endl;
            return Status::Success;
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "2️⃣ Check if door is locked" << std::endl;
            std::cout << "🔒 Door is locked!" << std::endl;
            return Status::Failure; // This stops the sequence
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "3️⃣ Walk through door (should not execute)" << std::endl;
            return Status::Success;
        })
    .end()
    .build();

std::cout << "Executing failing door sequence:" << std::endl;
Status door_result = failing_sequence.tick();
std::cout << "Result: " << (door_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
```

**Key Concepts:**
- Sequences execute children from left to right
- If a child returns `Failure`, the sequence immediately returns `Failure`
- If a child returns `Running`, the sequence returns `Running` and resumes from that child next tick
- If all children return `Success`, the sequence returns `Success`
- Perfect for step-by-step processes

---

## Lesson 4: Selector Nodes - Try This, Or That

Selectors try children in order until one succeeds. They're perfect for fallback behaviors.

```cpp
auto problem_solving = Builder()
    .selector()
        .action([](Blackboard& bb) -> Status {
            std::cout << "🔑 Try using the key" << std::endl;
            auto has_key = bb.get<bool>("has_key");
            if (has_key.value_or(false)) {
                std::cout << "✓ Door unlocked with key!" << std::endl;
                return Status::Success;
            } else {
                std::cout << "✗ No key available" << std::endl;
                return Status::Failure;
            }
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "🔓 Try picking the lock" << std::endl;
            // Simulate 70% success rate
            bool success = (rand() % 10) < 7;
            if (success) {
                std::cout << "✓ Lock picked successfully!" << std::endl;
                return Status::Success;
            } else {
                std::cout << "✗ Lock picking failed" << std::endl;
                return Status::Failure;
            }
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "🪓 Break down the door" << std::endl;
            std::cout << "✓ Door broken! (Always works but noisy)" << std::endl;
            return Status::Success; // Always succeeds as last resort
        })
    .end()
    .build();

std::cout << "Attempting to open a locked door (with key):" << std::endl;
problem_solving.blackboard().set("has_key", true);
Status door_open1 = problem_solving.tick();
std::cout << "Result: " << (door_open1 == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;

std::cout << "Attempting to open a locked door (without key):" << std::endl;
problem_solving.blackboard().set("has_key", false);
Status door_open2 = problem_solving.tick();
std::cout << "Result: " << (door_open2 == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
```

**Key Concepts:**
- Selectors try children from left to right
- If a child returns `Success`, the selector immediately returns `Success`
- If a child returns `Running`, the selector returns `Running` and resumes from that child next tick
- If all children return `Failure`, the selector returns `Failure`
- Perfect for fallback strategies and alternatives

---

## Lesson 5: Parallel Nodes - Do Multiple Things

Parallel nodes execute all children simultaneously. They use success and failure policies to determine outcome.

```cpp
auto multitasking = Builder()
    .parallel(Parallel::Policy::RequireOne, Parallel::Policy::RequireAll)
        .action([](Blackboard& bb) -> Status {
            std::cout << "🔍 Scanning for enemies... ";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "Clear!" << std::endl;
            return Status::Success;
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "❤️ Monitoring health... ";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "Healthy!" << std::endl;
            return Status::Success;
        })
        .action([](Blackboard& bb) -> Status {
            std::cout << "📡 Radio check... ";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            bool radio_works = (rand() % 2) == 0;
            if (radio_works) {
                std::cout << "Connected!" << std::endl;
                return Status::Success;
            } else {
                std::cout << "No signal!" << std::endl;
                return Status::Failure;
            }
        })
    .end()
    .build();

std::cout << "Running parallel tasks:" << std::endl;
Status parallel_result = multitasking.tick();
std::cout << "Result: " << (parallel_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
```

**Key Concepts:**
- Parallel nodes execute all children in the same tick
- Success policy determines when the parallel node succeeds:
  - `RequireOne`: Succeeds when at least one child succeeds
  - `RequireAll`: Succeeds when all children succeed
- Failure policy determines when the parallel node fails:
  - `RequireOne`: Fails when at least one child fails
  - `RequireAll`: Fails when all children fail
- Perfect for simultaneous monitoring and actions

---

## Lesson 6: Decorators - Modify Behavior

Decorators wrap other nodes and modify their behavior. They're like filters or modifiers for node results.

> **Tip:** Decorators always apply to the **next** node you declare—this can be an action or a composite such as `sequence()` or `selector()`. If you try to call `end()` or `build()` while a decorator/repeat/retry is still pending, Bonsai throws an error so you can fix the misplaced modifier immediately.

```cpp
// Inverter - flips success/failure
std::cout << "🔄 Inverter Demo:" << std::endl;
auto inverter_demo = Builder()
    .inverter()
        .action([](Blackboard& bb) -> Status {
            std::cout << "Action returns FAILURE" << std::endl;
            return Status::Failure; // Will be inverted to Success
        })
    .build();

Status inverted = inverter_demo.tick();
std::cout << "Inverter result: " << (inverted == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;

// Repeater - repeats successful actions
std::cout << "🔁 Repeater Demo:" << std::endl;
int repeat_counter = 0;
auto repeat_demo = Builder()
    .repeat(3)
        .action([&repeat_counter](Blackboard& bb) -> Status {
            repeat_counter++;
            std::cout << "Execution #" << repeat_counter << std::endl;
            return Status::Success; // Will be repeated 3 times
        })
    .build();

repeat_demo.tick();
std::cout << "Total executions: " << repeat_counter << std::endl;

// Retry - retries failed actions
std::cout << "🔄 Retry Demo:" << std::endl;
int retry_counter = 0;
auto retry_demo = Builder()
    .retry(3)
        .action([&retry_counter](Blackboard& bb) -> Status {
            retry_counter++;
            std::cout << "Attempt #" << retry_counter << std::endl;
            // Fail first 2 times, succeed on 3rd
            return (retry_counter < 3) ? Status::Failure : Status::Success;
        })
    .build();

Status retry_result = retry_demo.tick();
std::cout << "Retry result: " << (retry_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;

// Succeeder - always returns success
std::cout << "✅ Succeeder Demo:" << std::endl;
auto succeeder_demo = Builder()
    .succeeder()
        .action([](Blackboard& bb) -> Status {
            std::cout << "Action that fails but succeeder overrides" << std::endl;
            return Status::Failure; // Will become Success
        })
    .build();

Status succ_result = succeeder_demo.tick();
std::cout << "Succeeder result: " << (succ_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
```

**Available Decorators:**
- `inverter()`: Flips Success ↔ Failure (Running unchanged)
- `repeat(n)`: Repeats successful actions up to n times (-1 for infinite)
- `retry(n)`: Retries failed actions up to n times (-1 for infinite)
- `succeeder()`: Always returns Success regardless of child result
- `failer()`: Always returns Failure regardless of child result

**Repeat vs Retry:**
- **`repeat()`**: Use for looping behaviors like animations, patrols, or resource gathering
- **`retry()`**: Use for unreliable operations that might fail but should be attempted again

### Practical Examples

```cpp
// Example 1: Patrol behavior - repeat successful patrol route
auto patrol_tree = Builder()
    .repeat(5) // Patrol 5 times
        .sequence()
            .action([](Blackboard& bb) -> Status {
                std::cout << "🚶 Moving to checkpoint A" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "🔍 Scanning area" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "🚶 Moving to checkpoint B" << std::endl;
                return Status::Success;
            })
        .end()
    .build();

// Example 2: Network operation - retry failed connections
auto network_tree = Builder()
    .retry(3) // Try up to 3 times
        .action([](Blackboard& bb) -> Status {
            std::cout << "🌐 Attempting network connection..." << std::endl;
            // Simulate 70% failure rate
            if (rand() % 10 < 7) {
                std::cout << "❌ Connection failed" << std::endl;
                return Status::Failure;
            }
            std::cout << "✅ Connected successfully!" << std::endl;
            return Status::Success;
        })
    .build();

// Example 3: Animation loop - repeat indefinitely until interrupted
auto animation_tree = Builder()
    .repeat() // Infinite repeat
        .action([](Blackboard& bb) -> Status {
            auto should_stop = bb.get<bool>("stop_animation");
            if (should_stop.value_or(false)) {
                return Status::Failure; // Stop repeating
            }
            std::cout << "🎬 Playing animation frame" << std::endl;
            return Status::Success; // Continue repeating
        })
    .build();
```

---

## Lesson 7: Real-World Example - AI Guard

Let's build a complete AI guard behavior that demonstrates how all these concepts work together in practice.

```cpp
auto guard_ai = Builder()
    .selector() // Try different strategies
        // High priority: Handle emergencies
        .sequence()
            .action([](Blackboard& bb) -> Status {
                auto alert_level = bb.get<int>("alert_level");
                if (alert_level.value_or(0) >= 3) {
                    std::cout << "🚨 HIGH ALERT: Intruder detected!" << std::endl;
                    return Status::Success;
                }
                return Status::Failure;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "📞 Calling for backup" << std::endl;
                bb.set("backup_called", true);
                return Status::Success;
            })
        .end()
        
        // Medium priority: Investigate disturbances
        .sequence()
            .action([](Blackboard& bb) -> Status {
                auto alert_level = bb.get<int>("alert_level");
                if (alert_level.value_or(0) >= 2) {
                    std::cout << "🔍 Disturbance detected" << std::endl;
                    return Status::Success;
                }
                return Status::Failure;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "🚶 Moving to investigate" << std::endl;
                bb.set("investigating", true);
                return Status::Success;
            })
        .end()
        
        // Low priority: Normal patrol
        .sequence()
            .action([](Blackboard& bb) -> Status {
                std::cout << "🚶‍♂️ Normal patrol" << std::endl;
                return Status::Success;
            })
            .parallel(Parallel::Policy::RequireOne, Parallel::Policy::RequireAll)
                .action([](Blackboard& bb) -> Status {
                    std::cout << "   👀 Scanning area" << std::endl;
                    return Status::Success;
                })
                .action([](Blackboard& bb) -> Status {
                    std::cout << "   📝 Logging patrol" << std::endl;
                    return Status::Success;
                })
                .action([](Blackboard& bb) -> Status {
                    std::cout << "   📡 Radio check" << std::endl;
                    return rand() % 2 ? Status::Success : Status::Failure;
                })
            .end()
        .end()
    .end()
    .build();

// Simulate different scenarios
std::vector<int> scenarios = {0, 2, 3}; // Normal, disturbance, high alert
std::vector<std::string> scenario_names = {"Normal", "Disturbance", "High Alert"};

for (size_t i = 0; i < scenarios.size(); ++i) {
    std::cout << "--- Scenario: " << scenario_names[i] << " ---" << std::endl;
    guard_ai.blackboard().set("alert_level", scenarios[i]);
    Status guard_result = guard_ai.tick();
    std::cout << "Guard AI result: " << (guard_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
}
```

This example demonstrates:
- **Priority-based behavior**: Higher priority actions are checked first
- **State-driven logic**: The alert level drives different behaviors
- **Complex coordination**: Multiple systems working together
- **Realistic AI patterns**: Emergency response, investigation, and routine patrol

---

## Lesson 8: Manual Node Creation

For advanced cases, you can create nodes manually and use utility-based selection for sophisticated AI behavior.

```cpp
// Create a utility selector manually
auto utilitySelector = std::make_shared<UtilitySelector>();

// Add children with utility functions
utilitySelector->addChild(
    std::make_shared<Action>([](Blackboard& bb) -> Status {
        std::cout << "🍎 Eating action selected (high hunger utility)" << std::endl;
        return Status::Success;
    }),
    [](Blackboard& bb) -> float {
        return bb.get<double>("hunger").value_or(0.0);
    }
);

utilitySelector->addChild(
    std::make_shared<Action>([](Blackboard& bb) -> Status {
        std::cout << "😴 Sleeping action selected (high tiredness utility)" << std::endl;
        return Status::Success;
    }),
    [](Blackboard& bb) -> float {
        return bb.get<double>("tiredness").value_or(0.0);
    }
);

Tree utility_tree(utilitySelector);

// Test with different utility values
std::cout << "Testing utility-based selection:" << std::endl;

utility_tree.blackboard().set("hunger", 0.8);
utility_tree.blackboard().set("tiredness", 0.3);
std::cout << "High hunger, low tiredness:" << std::endl;
utility_tree.tick(); // Should select eating

utility_tree.blackboard().set("hunger", 0.2);
utility_tree.blackboard().set("tiredness", 0.9);
std::cout << "Low hunger, high tiredness:" << std::endl;
utility_tree.tick(); // Should select sleeping
```

**Key Concepts:**
- Manual node creation gives you full control
- Utility-based selection chooses actions based on scored utility functions
- Custom node types can implement sophisticated AI behaviors
- Mix manual and builder approaches as needed

---

## Lesson 9: Comprehensive Examples - All Features

This section contains extensive examples demonstrating every feature of the Bonsai library. These examples were moved from README.md to keep the README concise.

### Advanced Node Types and Patterns

```cpp
#include <bonsai/bonsai.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

using namespace bonsai;

int main() {
    std::cout << "Comprehensive Bonsai Behavior Tree Examples" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    // ========================================
    // 1. BLACKBOARD USAGE - Shared data between nodes
    // ========================================
    
    std::cout << "\n=== Blackboard Usage ===" << std::endl;
    
    auto blackboard_tree = Builder()
        .sequence()
            .action([](Blackboard& bb) -> Status {
                bb.set("player_health", 75);
                bb.set("enemy_count", 3);
                bb.set("weapon_type", std::string("sword"));
                std::cout << "Set player health: 75, enemies: 3, weapon: sword" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                auto health = bb.get<int>("player_health");
                auto enemies = bb.get<int>("enemy_count");
                auto weapon = bb.get<std::string>("weapon_type");
                
                std::cout << "Retrieved - Health: " << (health.has_value() ? health.value() : 0);
                std::cout << ", Enemies: " << (enemies.has_value() ? enemies.value() : 0);
                std::cout << ", Weapon: " << (weapon.has_value() ? weapon.value() : "none") << std::endl;
                
                return Status::Success;
            })
        .end()
        .build();
    
    std::cout << "Executing blackboard example:" << std::endl;
    Status bb_result = blackboard_tree.tick();
    std::cout << "Blackboard result: " << (bb_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // ========================================
    // 2. ADVANCED SEQUENCE PATTERNS
    // ========================================
    
    std::cout << "\n=== Advanced Sequence Patterns ===" << std::endl;
    
    auto sequence_tree = Builder()
        .sequence()
            .action([](Blackboard& bb) -> Status {
                std::cout << "Step 1: Check prerequisites" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Step 2: Initialize resources" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Step 3: Execute main task" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Step 4: Cleanup" << std::endl;
                return Status::Success;
            })
        .end()
        .build();
    
    std::cout << "Executing sequence (all should succeed):" << std::endl;
    Status seq_result = sequence_tree.tick();
    std::cout << "Sequence result: " << (seq_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // ========================================
    // 3. ADVANCED SELECTOR PATTERNS
    // ========================================
    
    std::cout << "\n=== Advanced Selector Patterns ===" << std::endl;
    
    auto selector_tree = Builder()
        .selector()
            .action([](Blackboard& bb) -> Status {
                std::cout << "Option 1: Check if we have melee weapon (fails)" << std::endl;
                bb.set("has_melee_weapon", false);
                auto has_weapon = bb.get<bool>("has_melee_weapon");
                if (has_weapon.has_value() && has_weapon.value()) {
                    std::cout << "Use melee attack!" << std::endl;
                    return Status::Success;
                }
                return Status::Failure;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Option 2: Check if we have ranged weapon" << std::endl;
                // Simulate having ranged weapon
                bb.set("has_ranged_weapon", true);
                auto has_weapon = bb.get<bool>("has_ranged_weapon");
                if (has_weapon.has_value() && has_weapon.value()) {
                    std::cout << "Use ranged attack!" << std::endl;
                    return Status::Success;
                }
                return Status::Failure;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Option 3: Retreat (fallback option)" << std::endl;
                return Status::Success; // Always succeeds as last resort
            })
        .end()
        .build();
    
    std::cout << "Executing selector (should succeed on option 2):" << std::endl;
    Status sel_result = selector_tree.tick();
    std::cout << "Selector result: " << (sel_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // ========================================
    // 4. PARALLEL NODES - Execute children simultaneously
    // ========================================
    
    std::cout << "\n=== Parallel Nodes ===" << std::endl;
    
    // Parallel node that succeeds when 2 out of 3 children succeed
    auto parallel_tree = Builder()
        .parallel(2) // Success threshold = 2
            .action([](Blackboard& bb) -> Status {
                std::cout << "Parallel task 1: Scanning for enemies" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Parallel task 2: Monitoring health" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                std::cout << "Parallel task 3: Communication (fails)" << std::endl;
                return Status::Failure;
            })
        .end()
        .build();
    
    std::cout << "Executing parallel (2/3 success threshold):" << std::endl;
    Status par_result = parallel_tree.tick();
    std::cout << "Parallel result: " << (par_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // ========================================
    // 5. DECORATOR NODES - Modify child behavior
    // ========================================
    
    std::cout << "\n=== Decorator Nodes ===" << std::endl;
    
    // Inverter - Flips success/failure
    auto inverter_tree = Builder()
        .inverter()
            .action([](Blackboard& bb) -> Status {
                std::cout << "Action that fails" << std::endl;
                return Status::Failure; // Will be inverted to Success
            })
        .end()
        .build();
    
    std::cout << "Executing inverter (failure becomes success):" << std::endl;
    Status inv_result = inverter_tree.tick();
    std::cout << "Inverter result: " << (inv_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // Succeeder - Always returns success
    auto succeeder_tree = Builder()
        .succeeder()
            .action([](Blackboard& bb) -> Status {
                std::cout << "Action that fails but succeeder overrides" << std::endl;
                return Status::Failure; // Will become Success
            })
        .end()
        .build();
    
    std::cout << "\nExecuting succeeder:" << std::endl;
    Status succ_result = succeeder_tree.tick();
    std::cout << "Succeeder result: " << (succ_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // Repeater - Executes child multiple times
    int repeat_count = 0;
    auto repeat_tree = Builder()
        .repeat(3)
            .action([&repeat_count](Blackboard& bb) -> Status {
                repeat_count++;
                std::cout << "Repeat execution #" << repeat_count << std::endl;
                return Status::Success;
            })
        .end()
        .build();
    
    std::cout << "\nExecuting repeater (3 times):" << std::endl;
    Status rep_result = repeat_tree.tick();
    std::cout << "Repeat result: " << (rep_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // ========================================
    // 6. UTILITY-BASED SELECTION - AI Decision Making
    // ========================================
    
    std::cout << "\n=== Utility-Based Selection ===" << std::endl;
    
    auto utility_tree = Builder()
        .utilitySelector()
            .action([](Blackboard& bb) -> Status {
                // Calculate utility for attacking
                auto health = bb.get<int>("player_health");
                auto enemy_count = bb.get<int>("enemy_count");
                
                double utility = 0.0;
                if (health.has_value() && enemy_count.has_value()) {
                    // Higher utility when health is high and enemies are few
                    utility = (health.value() / 100.0) * (1.0 / enemy_count.value());
                }
                
                bb.set("utility_score", utility);
                std::cout << "Attack option utility: " << utility << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                // Calculate utility for defending
                auto health = bb.get<int>("player_health");
                auto enemy_count = bb.get<int>("enemy_count");
                
                double utility = 0.0;
                if (health.has_value() && enemy_count.has_value()) {
                    // Higher utility when health is low or enemies are many
                    utility = (1.0 - health.value() / 100.0) + (enemy_count.value() / 10.0);
                }
                
                bb.set("utility_score", utility);
                std::cout << "Defend option utility: " << utility << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                // Calculate utility for fleeing
                auto health = bb.get<int>("player_health");
                auto enemy_count = bb.get<int>("enemy_count");
                
                double utility = 0.0;
                if (health.has_value() && enemy_count.has_value()) {
                    // Higher utility when health is very low
                    utility = (health.value() < 30) ? 0.9 : 0.1;
                }
                
                bb.set("utility_score", utility);
                std::cout << "Flee option utility: " << utility << std::endl;
                return Status::Success;
            })
        .end()
        .build();
    
    std::cout << "Executing utility selector:" << std::endl;
    Status util_result = utility_tree.tick();
    std::cout << "Utility selector result: " << (util_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    
    // ========================================
    // 7. WEIGHTED RANDOM SELECTION
    // ========================================
    
    std::cout << "\n=== Weighted Random Selection ===" << std::endl;
    
    auto weighted_tree = Builder()
        .weightedRandomSelector()
            .action([](Blackboard& bb) -> Status {
                bb.set("weight", 3.0); // High weight - more likely
                std::cout << "Common action (weight: 3.0)" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                bb.set("weight", 1.0); // Medium weight
                std::cout << "Uncommon action (weight: 1.0)" << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) -> Status {
                bb.set("weight", 0.5); // Low weight - less likely
                std::cout << "Rare action (weight: 0.5)" << std::endl;
                return Status::Success;
            })
        .end()
        .build();
    
    std::cout << "Executing weighted random selector (3 times):" << std::endl;
    for (int i = 0; i < 3; ++i) {
        Status weight_result = weighted_tree.tick();
        std::cout << "  Weighted result " << (i+1) << ": " << (weight_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    }
    
    // ========================================
    // 8. COMPLEX NESTED TREES - Game AI Example
    // ========================================
    
    std::cout << "\n=== Complex Nested Trees ===" << std::endl;
    
    // Realistic AI behavior tree for a game character
    auto game_ai_tree = Builder()
        .selector() // Try different strategies
            // Strategy 1: Aggressive (when health is high)
            .sequence()
                .action([](Blackboard& bb) -> Status {
                    auto health = bb.get<int>("player_health");
                    if (health.has_value() && health.value() > 70) {
                        std::cout << "Health is high, being aggressive" << std::endl;
                        return Status::Success;
                    }
                    return Status::Failure;
                })
                .selector() // Choose attack type
                    .sequence() // Melee attack
                        .action([](Blackboard& bb) -> Status {
                            std::cout << "Check if enemy is close" << std::endl;
                            bb.set("enemy_distance", 2.0);
                            auto distance = bb.get<double>("enemy_distance");
                            return (distance.has_value() && distance.value() < 3.0) ? Status::Success : Status::Failure;
                        })
                        .action([](Blackboard& bb) -> Status {
                            std::cout << "Executing melee attack!" << std::endl;
                            return Status::Success;
                        })
                    .end()
                    .sequence() // Ranged attack
                        .action([](Blackboard& bb) -> Status {
                            std::cout << "Check if we have ranged weapon" << std::endl;
                            return Status::Success; // Assume we have it
                        })
                        .action([](Blackboard& bb) -> Status {
                            std::cout << "Executing ranged attack!" << std::endl;
                            return Status::Success;
                        })
                    .end()
                .end()
            .end()
            
            // Strategy 2: Defensive (when health is medium)
            .sequence()
                .action([](Blackboard& bb) -> Status {
                    auto health = bb.get<int>("player_health");
                    if (health.has_value() && health.value() >= 30 && health.value() <= 70) {
                        std::cout << "Health is medium, being defensive" << std::endl;
                        return Status::Success;
                    }
                    return Status::Failure;
                })
                .parallel(1) // Do defensive actions simultaneously
                    .action([](Blackboard& bb) -> Status {
                        std::cout << "Raising shield" << std::endl;
                        return Status::Success;
                    })
                    .action([](Blackboard& bb) -> Status {
                        std::cout << "Looking for cover" << std::endl;
                        return Status::Success;
                    })
                .end()
            .end()
            
            // Strategy 3: Retreat (when health is low)
            .sequence()
                .action([](Blackboard& bb) -> Status {
                    auto health = bb.get<int>("player_health");
                    if (health.has_value() && health.value() < 30) {
                        std::cout << "Health is low, retreating!" << std::endl;
                        return Status::Success;
                    }
                    return Status::Failure;
                })
                .action([](Blackboard& bb) -> Status {
                    std::cout << "Finding escape route" << std::endl;
                    return Status::Success;
                })
                .action([](Blackboard& bb) -> Status {
                    std::cout << "Running away!" << std::endl;
                    return Status::Success;
                })
            .end()
        .end()
        .build();
    
    // Test with different health values
    std::vector<int> health_values = {90, 50, 20};
    for (int health : health_values) {
        std::cout << "\nTesting AI with health: " << health << std::endl;
        game_ai_tree.blackboard().set("player_health", health);
        Status ai_result = game_ai_tree.tick();
        std::cout << "AI decision result: " << (ai_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    }
    
    // ========================================
    // 9. ADVANCED PATTERNS AND STATE MACHINES
    // ========================================
    
    std::cout << "\n=== Advanced Patterns ===" << std::endl;
    
    // State machine using selector + sequences
    auto state_machine = Builder()
        .selector()
            // Idle state
            .sequence()
                .action([](Blackboard& bb) -> Status {
                    auto state = bb.get<std::string>("current_state");
                    if (!state.has_value() || state.value() == "idle") {
                        bb.set("current_state", std::string("idle"));
                        std::cout << "State: IDLE - Waiting for input" << std::endl;
                        
                        // Simulate input
                        bb.set("input_received", true);
                        return Status::Success;
                    }
                    return Status::Failure;
                })
                .action([](Blackboard& bb) -> Status {
                    auto input = bb.get<bool>("input_received");
                    if (input.has_value() && input.value()) {
                        bb.set("current_state", std::string("processing"));
                        return Status::Success;
                    }
                    return Status::Failure;
                })
            .end()
            
            // Processing state
            .sequence()
                .action([](Blackboard& bb) -> Status {
                    auto state = bb.get<std::string>("current_state");
                    if (state.has_value() && state.value() == "processing") {
                        std::cout << "State: PROCESSING - Handling input" << std::endl;
                        return Status::Success;
                    }
                    return Status::Failure;
                })
                .action([](Blackboard& bb) -> Status {
                    // Simulate processing
                    std::cout << "Processing complete" << std::endl;
                    bb.set("current_state", std::string("idle"));
                    bb.set("input_received", false);
                    return Status::Success;
                })
            .end()
        .end()
        .build();
    
    std::cout << "Executing state machine:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        std::cout << "Cycle " << (i+1) << ":" << std::endl;
        Status sm_result = state_machine.tick();
        std::cout << "State machine result: " << (sm_result == Status::Success ? "SUCCESS" : "FAILURE") << std::endl;
    }
    
    std::cout << "\n=== Comprehensive Examples Complete ===" << std::endl;
    std::cout << "This comprehensive example demonstrated:" << std::endl;
    std::cout << "• Blackboard for shared data storage" << std::endl;
    std::cout << "• Advanced sequence and selector patterns" << std::endl;
    std::cout << "• Parallel execution" << std::endl;
    std::cout << "• Decorator nodes (Inverter, Succeeder, Repeat, Retry)" << std::endl;
    std::cout << "• Utility-based and weighted random selection" << std::endl;
    std::cout << "• Complex nested tree structures" << std::endl;
    std::cout << "• Real-world AI behavior patterns" << std::endl;
    std::cout << "• State machine implementations" << std::endl;
    
    return 0;
}
```

### Performance Tips and Best Practices

1. **Blackboard Usage**: Store only necessary data to minimize memory overhead
2. **Tree Depth**: Keep trees reasonably shallow for better performance
3. **Action Complexity**: Keep individual actions simple and focused
4. **Memory Management**: Use smart pointers for complex data types
5. **Threading**: The blackboard is thread-safe, but be careful with shared state

### Common Patterns

#### Guard Conditions
```cpp
auto tree = Builder()
    .sequence()
        .action([](Blackboard& bb) -> Status {
            // Guard condition
            auto health = bb.get<int>("health");
            return (health.has_value() && health.value() > 0) ? Status::Success : Status::Failure;
        })
        .action([](Blackboard& bb) -> Status {
            // Main action only executes if guard passes
            std::cout << "Executing main action" << std::endl;
            return Status::Success;
        })
    .end()
    .build();
```

#### Retry Logic
```cpp
auto retry_tree = Builder()
    .repeat(3) // Try up to 3 times
        .action([](Blackboard& bb) -> Status {
            // Action that might fail
            auto success_chance = bb.get<double>("success_chance").value_or(0.5);
            return (rand() < success_chance * RAND_MAX) ? Status::Success : Status::Failure;
        })
    .end()
    .build();
```

This completes our comprehensive tutorial covering all aspects of the Bonsai behavior tree library!
