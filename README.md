<img align="right" width="26%" src="./misc/logo.png"> 

# Bonsai

A lightweight, header-only C++20 behavior tree library designed for AI decision-making systems.

## Features

- **Header-only**: Simple integration, just include the main header
- **Modern C++20**: Takes advantage of concepts, ranges, and other modern features
- **Thread-safe**: Blackboard provides safe concurrent access to shared data
- **Extensible**: Easy to add custom node types and behaviors
- **Fluent API**: Builder pattern for intuitive tree construction

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

### Simple Example

```cpp
#include <bonsai/bonsai.hpp>

using namespace bonsai;

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

## Core Concepts Reference

### Status Values
- `Status::Success` - Node completed successfully
- `Status::Failure` - Node failed to complete  
- `Status::Running` - Node is still executing (for async operations)

### Node Types

#### Composite Nodes
- **Sequence**: Executes children in order, succeeds when all succeed
- **Selector**: Tries children in order, succeeds when one succeeds  
- **Parallel**: Executes all children simultaneously

#### Decorator Nodes
- **Inverter**: Flips Success ↔ Failure
- **Repeater**: Retries child on failure
- **Timeout**: Fails child after time limit
- **Cooldown**: Prevents execution during cooldown period

#### Leaf Nodes
- **Action**: Executes custom logic

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

This is a header-only library. Simply include the main header:

```cpp
#include <bonsai/bonsai.hpp>
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
- **`main.cpp`** - Robot navigation with PID controllers
- **`utility_demo.cpp`** - AI decision making with utility functions  
- **`comprehensive_test.cpp`** - Full feature test suite
- **`pea_harvester_demo.cpp`** - Complex real-world optimization scenario

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.15+ (for building examples)
- Standard library with threading support

## License

MIT License - see LICENSE file for details.
