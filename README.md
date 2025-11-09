<img align="right" width="26%" src="./misc/logo.png"> 

# Bonsai

A lightweight C++20 AI orchestration toolkit that ships both behavior trees *and* finite state machines backed by the same blackboard.

## Features

- **Dual decision systems** – Compose complex logic with `tree::Builder` behavior trees or `state::Builder` finite state machines without leaving the API surface.
- **Shared blackboard** – Thread-safe data store that both systems use for coordination, event passing, and debugging hooks.
- **Modern C++20** – Uses concepts, ranges, lambdas, and strong typing throughout the public API.
- **Lightweight & extensible** – Include `<bonsai/bonsai.hpp>`, derive custom nodes/states, or inject callbacks.
- **Fluent builders** – Chainable DSL for declaring sequences/selectors, decorators, states, transitions, and guards in one place.
- **Production niceties** – Repeat/retry decorators, timeout/cooldown support, scoped blackboard overrides, guard/cannot happen transitions, and lifecycle observability baked in.

## 🚀 Quick Start

**New to behavior trees?** Check out our comprehensive tutorial:

📖 **[Complete Tutorial Guide](TUTORIAL.md)** - Step-by-step learning with examples

Or run the interactive tutorial:

```bash
# Build and run the comprehensive tutorial
cmake -DBONSAI_BUILD_EXAMPLES=ON . -B build
cmake --build build
./build/getting_started_tutorial
```

### Behavior Tree in 20 Lines

```cpp
#include <bonsai/bonsai.hpp>

using namespace bonsai::tree;

int main() {
    // Create a simple behavior tree
    auto tree = Builder()
        .sequence()
            .action([](Blackboard& bb) {
                std::cout << "Starting task..." << std::endl;
                return Status::Success;
            })
            .action([](Blackboard& bb) {
                std::cout << "Task completed!" << std::endl;
                return Status::Success;
            })
        .end()
        .build();
    
    // Execute the tree
    Status result = tree.tick();
    
    return 0;
}
```

### Finite State Machine Snapshot

```cpp
#include <bonsai/bonsai.hpp>

using namespace bonsai;

int main() {
    auto machine = state::Builder()
                       .initial("idle")
                       .state("idle")
                       .onEnter([](tree::Blackboard &) { std::puts("🧍 idle"); })
                       .transitionTo("run", [](tree::Blackboard &bb) { return bb.get<bool>("wants_run").value_or(false); })
                       .state("run")
                       .onEnter([](tree::Blackboard &bb) {
                           std::puts("🏃 sprinting");
                           bb.set("stamina", 100);
                       })
                       .onUpdate([](tree::Blackboard &bb) {
                           auto stamina = bb.get<int>("stamina").value_or(0);
                           bb.set("stamina", stamina - 10);
                       })
                       .transitionTo("idle", [](tree::Blackboard &bb) { return bb.get<int>("stamina").value_or(0) <= 0; })
                       .build();

    machine->blackboard().set("wants_run", true);
    for (int i = 0; i < 15; ++i) {
        machine->tick();
    }
}
```

### Mini Cookbook (lifted from `examples/`)

#### Battery-aware robot (behavior tree)

```cpp
auto tree = tree::Builder()
                .selector()
                .sequence() // charge when low
                .action([&robot](Blackboard &) {
                    return robot.battery < 20.0 ? Status::Success : Status::Failure;
                })
                .action([&robot](Blackboard &) {
                    robot.isCharging = true;
                    return robot.battery >= 90.0 ? Status::Success : Status::Running;
                })
                .end()
                .action([&pid](Blackboard &) { return pid.update(); }) // otherwise chase PID target
                .end()
                .build();
```

#### Utility selector (behavior tree)

```cpp
using namespace bonsai::tree;

auto selector = std::make_shared<UtilitySelector>();
selector->addChild(
    std::make_shared<Action>([](Blackboard &) {
        std::puts("🍎 Eat");
        return Status::Success;
    }),
    [](Blackboard &bb) { return bb.get<float>("hunger").value_or(0.0f); }
);
selector->addChild(
    std::make_shared<Action>([](Blackboard &) {
        std::puts("💤 Sleep");
        return Status::Success;
    }),
    [](Blackboard &bb) { return bb.get<float>("tiredness").value_or(0.0f); }
);

Tree brain(selector);
brain.blackboard().set("hunger", 0.8f);
brain.tick(); // Picks the action with the highest utility
```

#### Guarded door lock (state machine)

```cpp
auto door = state::Builder()
                .initial("locked")
                .state("locked")
                .transitionTo("unlocked", [](tree::Blackboard &bb) { return bb.has("pin"); })
                .state("unlocked")
                .onGuard([](tree::Blackboard &bb) {
                    return bb.get<std::string>("pin").value_or("") == "1234";
                })
                .onEnter([](tree::Blackboard &) { std::puts("Door opened"); })
                .build();
door->blackboard().set("pin", "1234");
door->tick(); // Guard passes, enter unlocked state
```

#### Timeout and cooldown decorators (behavior tree)

```cpp
using namespace bonsai::tree;

// Timeout: fail if action takes too long
auto tree = Builder()
                .decorator(decorators::Timeout(5.0f)) // 5 second timeout
                .action([](Blackboard &bb) {
                    // Long-running operation
                    return Status::Running;
                })
                .build();

// Cooldown: prevent rapid re-execution
auto cooldown_tree = Builder()
                         .decorator(decorators::Cooldown(2.0f)) // 2 second cooldown
                         .action([](Blackboard &bb) {
                             std::puts("Special attack!");
                             return Status::Success;
                         })
                         .build();
```

## Core Concepts Reference

### Behavior Tree Status Values

- `Status::Success` - Node completed successfully
- `Status::Failure` - Node failed to complete  
- `Status::Running` - Node is still executing (for async operations)

### Behavior Tree Node Types

#### Composite Nodes
- **Sequence** (`.sequence()`): Executes children in order, succeeds when all succeed
- **Selector** (`.selector()`): Tries children in order, succeeds when one succeeds  
- **Parallel** (`.parallel(policy, policy)` or `.parallel(threshold)`): Executes all children simultaneously

#### Decorator Nodes
- **Inverter** (`.inverter()`): Flips Success ↔ Failure
- **Succeeder** (`.succeeder()`): Always returns Success (except Running)
- **Failer** (`.failer()`): Always returns Failure (except Running)
- **Repeat** (`.repeat(n)`): Repeats successful child N times (or infinitely if n=-1)
- **Retry** (`.retry(n)`): Retries failed child N times (or infinitely if n=-1)
- **Custom** (`.decorator(func)`): Apply custom decorator function
  - Available in `decorators::` namespace: `Timeout(seconds)`, `Cooldown(seconds)`, etc.

#### Leaf Nodes
- **Action** (`.action(func)`): Executes custom logic

#### Advanced Nodes (Manual Creation)
- **UtilitySelector**: Selects child with highest utility score
- **WeightedRandomSelector**: Randomly selects child weighted by utility scores

### State Machine Building Blocks

- **States** – Provide `onGuard`, `onEnter`, `onUpdate`, and `onExit` callbacks. Use `state::CbState::create` for quick lambda-only states.
- **Transitions** – Attach `transitionTo("target", condition)` to the current state. Conditions receive the shared blackboard.
- **Special transitions** – `.ignoreEvent()` keeps the current state; `.cannotHappen()` raises if triggered for easier debugging.
- **Lifecycle** – `state::StateMachine::tick()` evaluates guards, runs `onEnter` once per transition, and pumps `onUpdate` each tick.

### Blackboard

Thread-safe shared memory system for communication between nodes:

```cpp
// Store data of any type
tree.blackboard().set("health", 100);
tree.blackboard().set("name", std::string("Player"));

// Retrieve data safely with std::optional
auto health = tree.blackboard().get<int>("health");
if (health.has_value() && health.value() < 50) {
    // Low health logic
}

// Use scoped overrides that automatically roll back
{
    auto scope = tree.blackboard().pushScope();
    tree.blackboard().set("health", 10);
    // ...
} // scope popped here, health returns to previous value

// Observe all accesses for debugging
tree.blackboard().setObserver([](const Blackboard::Event &evt) {
    std::cout << "Blackboard event: " << static_cast<int>(evt.type) << " key=" << evt.key << std::endl;
});
```

## Building

Include the main header and link against the library:

```cpp
#include <bonsai/bonsai.hpp>
```

Both behavior trees and state machines are available out of the box:

```cpp
bonsai::tree::Builder btBuilder;
bonsai::state::Builder smBuilder;
```

### Using CMake

```cmake
find_package(bonsai REQUIRED)
target_link_libraries(your_target bonsai::bonsai)
```

Or use FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
    bonsai
    GIT_REPOSITORY https://github.com/your-repo/bonsai
    GIT_TAG main
)
FetchContent_MakeAvailable(bonsai)
target_link_libraries(your_target bonsai::bonsai)
```

### Building Examples

```bash
mkdir build && cd build
cmake -DBONSAI_BUILD_EXAMPLES=ON ..
make

# Run the comprehensive tutorial
./getting_started_tutorial

# Run other examples
./main
./utility_demo
./comprehensive_test
```

## Example Projects

The `examples/` directory contains comprehensive demonstrations:

- **`getting_started_tutorial.cpp`** - 🌟 **START HERE!** Interactive step-by-step tutorial
- **`state_machine_demo.cpp`** - Traffic lights, characters, and enemy AI finite state machines
- **`state_lifecycle_demo.cpp`** - Guard/enter/update/exit walkthrough with door-lock logic
- **`main.cpp`** - Robot navigation with PID controllers
- **`utility_demo.cpp`** - AI decision making with utility and weighted random selectors
- **`builder_utility_demo.cpp`** - Demonstrates manual node creation with utility selectors
- **`comprehensive_test.cpp`** - Full feature test suite
- **`pea_harvester_demo_fixed.cpp`** - Complex real-world optimization scenario

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.15+ (for building examples)
- Standard library with threading support

## License

MIT License - see LICENSE file for details.
