<img align="right" width="26%" src="./misc/logo.png"> 

# Bonsai

High-performance C++20 behavior trees and hierarchical state machines with parallel execution.

## Features

- **Behavior Trees & State Machines** – Two AI systems sharing a thread-safe blackboard
- **Hierarchical States** – Composite states with nested machines and parallel regions
- **Parallel Execution** – Built-in thread pool with early-stop optimization
- **Modern C++20** – Async/await, coroutines, concepts, move semantics
- **Production Ready** – Timeout/retry decorators, utility AI, 90% test coverage

## Quick Start

📖 **[Tutorial Guide](TUTORIAL.md)** | 🎮 **[Interactive Demo](examples/getting_started_tutorial.cpp)**

```bash
cmake -B build -DBONSAI_BUILD_EXAMPLES=ON
cmake --build build
./build/getting_started_tutorial
```

## Examples

### Simple Behavior Tree

```cpp
#include <bonsai/bonsai.hpp>
using namespace bonsai::tree;

auto tree = Builder()
    .sequence()
        .action([](Blackboard& bb) {
            std::cout << "Hello ";
            return Status::Success;
        })
        .action([](Blackboard& bb) {
            std::cout << "World!\n";
            return Status::Success;
        })
    .end()
    .build();

tree.tick(); // Prints: Hello World!
```

### State Machine with Parallel Regions

```cpp
using namespace bonsai::state;

auto machine = Builder()
    .compositeState("Combat", CompositeState::HistoryType::Deep,
        [](Builder& b) {
            // Main combat logic
            b.state("Attack")
                .transitionTo("Defend", [](auto& bb) {
                    return bb.get<int>("health").value_or(100) < 30;
                })
             .state("Defend");
            b.initial("Attack");
        })
    .region("Weapons", [](Builder& r) {
        // Parallel weapon system
        r.state("Ready")
            .transitionTo("Reload", [](auto& bb) {
                return bb.get<int>("ammo").value_or(0) == 0;
            });
        r.initial("Ready");
    })
    .initial("Combat")
    .build();
```

### Utility AI Selector

```cpp
auto selector = std::make_shared<UtilitySelector>();

selector->addChild(eatAction, [](auto& bb) {
    return bb.get<float>("hunger").value_or(0.0f);
});
selector->addChild(sleepAction, [](auto& bb) {
    return bb.get<float>("tiredness").value_or(0.0f);  
});

tree.tick(); // Executes highest utility action
```

## Core Concepts

### Behavior Trees
- **Composites**: `Sequence`, `Selector`, `Parallel`
- **Decorators**: `Timeout`, `Retry`, `Repeat`, `Inverter`
- **Leaf**: `Action` (sync/async/coroutine)
- **Status**: `Success`, `Failure`, `Running`

### State Machines
- **States**: Guard, Enter, Update, Exit callbacks
- **Transitions**: Conditional with priorities
- **Composite**: Nested machines, parallel regions
- **History**: None, Shallow, Deep

### Blackboard
```cpp
// Thread-safe data store
bb.set("health", 100);
auto hp = bb.get<int>("health"); // Returns std::optional

// Scoped overrides
{
    auto scope = bb.pushScope();
    bb.set("temp", true);
} // Automatically restored
```

## Installation

```cmake
# Using FetchContent
include(FetchContent)
FetchContent_Declare(bonsai
    GIT_REPOSITORY https://github.com/your-repo/bonsai
    GIT_TAG main)
FetchContent_MakeAvailable(bonsai)
target_link_libraries(your_target bonsai::bonsai)
```

## Building

```bash
mkdir build && cd build
cmake .. -DBONSAI_BUILD_EXAMPLES=ON -DBONSAI_ENABLE_TESTS=ON
make -j
make test  # Run tests
```

## Requirements

- C++20 (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.15+

## License

MIT