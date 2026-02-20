<img align="right" width="26%" src="./misc/logo.png">

# Stateup

C++20 library for **behavior trees**, **hierarchical state machines**, and **Datalog reasoning** — three complementary paradigms for decision-making and reactive control, sharing a thread-safe blackboard.

## Features

- **Behavior Trees** — Sequence, Selector, Parallel, Decorator, Utility AI, reactive nodes
- **State Machines** — Composite states, parallel regions, deep history, timed transitions
- **Datalog Engine** — Semi-naive evaluation, transitive closure, joins, aggregation, secondary indexes
- **Shared Blackboard** — Type-safe, scoped, thread-safe data store across all three systems
- **Parallel Execution** — Built-in thread pool with early-stop and bulk operations
- **Modern C++20** — Coroutines, concepts, `std::span`, `std::ranges`, move semantics

## Quick Start

```bash
make config && make build
./build/getting_started_tutorial   # behavior tree walkthrough
./build/logic_demo                 # datalog engine demos
```

## Namespaces

```cpp
#include <stateup/stateup.hpp>

namespace su  = stateup;          // short alias
namespace sut = stateup::tree;
namespace sus = stateup::state;
namespace sul = stateup::logic;
```

---

## Behavior Trees

```cpp
using namespace stateup::tree;

auto tree = Builder()
    .sequence()
        .action([](Blackboard& bb) {
            bb.set("step", 1);
            return Status::Success;
        })
        .action([](Blackboard& bb) -> task<Status> {
            co_await some_async_op();
            co_return Status::Success;
        })
    .end()
    .build();

tree.tick();
```

**Node types:**

| Category | Nodes |
|---|---|
| Composites | `Sequence`, `Selector`, `Parallel` |
| Decorators | `Inverter`, `Retry`, `Repeat`, `Timeout` |
| Leaf | `Action` (sync / async / coroutine) |
| Advanced | `ReactiveSequence`, `MemorySequence`, `DynamicSelector`, `UtilitySelector` |

---

## State Machines

```cpp
using namespace stateup::state;

auto machine = Builder()
    .initial("idle")
    .state("idle")
        .onEnter([](auto& bb) { bb.set("speed", 0); })
        .transitionTo("running", [](auto& bb) {
            return bb.get<bool>("start_cmd").value_or(false);
        })
    .state("running")
        .onUpdate([](auto& bb) { bb.set("speed", 10); })
        .transitionTo("idle", [](auto& bb) {
            return bb.get<bool>("stop_cmd").value_or(false);
        })
    .build();

machine->tick();
```

**Features:** composite states, parallel regions, deep/shallow history, timed transitions, guard conditions, transition priorities.

---

## Datalog Engine (`stateup::logic`)

Semi-naive evaluation over immutable sorted relations — a C++20 port of [Zodd](https://github.com/). Designed for recursive queries: reachability, role hierarchies, dependency resolution, permission inference.

### Core types

```cpp
using namespace stateup::logic;

// Tuple type — must satisfy TupleLike (operator< + operator==, trivially copyable)
struct Edge { int from, to; auto operator<=>(const Edge&) const = default; };

// Immutable sorted deduplicated set
auto edges = Relation<Edge>::from_slice({{1,2},{2,3},{3,4}});

// Delta variable for semi-naive fixpoint
Variable<Edge> v;
v.insert_slice(edges.elements());
```

### Transitive closure

```cpp
Iteration<Edge> iter;
auto* edges     = iter.variable();
auto* reachable = iter.variable();

edges->insert_slice(base_edges);
reachable->insert_slice(base_edges);

while (iter.changed()) {
    // reachable(A,C) :- reachable(A,B), edges(B,C)
    join_into<Edge, Edge, Edge, int>(
        *reachable, *edges, reachable,
        [](const Edge& e) { return e.to;   },   // join key from reachable
        [](const Edge& e) { return e.from; },   // join key from edges
        [](const Edge& r, const Edge& e) -> Edge { return {r.from, e.to}; }
    );
}

auto result = reachable->complete();  // all (A,C) reachable pairs
```

### Aggregation

```cpp
auto totals = aggregate<Record, int, int>(
    std::span<const Record>(records),
    [](const Record& r) { return r.category; },  // group key
    [](const Record& r) { return r.value; },      // value to fold
    [](int a, int b) { return a + b; },           // fold function
    0                                              // identity
);
```

### Secondary index

```cpp
auto key_ext = [](const Log& l) { return l.node_id; };
SecondaryIndex idx(logs, key_ext);          // CTAD: Key deduced automatically

auto span   = idx.get(42);                  // point lookup  — O(log N)
auto spans  = idx.get_range(10, 20);        // range query   — O(log N + K)
```

### Multi-way leapfrog join

```cpp
auto leaper = make_extend_with<PrefixTuple>(source_relation,
    [](const PrefixTuple& p) { return p.key; },   // key from prefix
    [](const SourceTuple& s) { return s.key; },   // key from source
    [](const SourceTuple& s) { return s.val; }    // value to extend with
);

std::vector<Leaper<PrefixTuple, int>*> leapers = {&leaper};
extend_into(source, std::span(leapers), &output,
    [](const PrefixTuple& p, int v) -> OutputTuple { return {p.id, v}; });
```

**Module summary:**

| File | Purpose |
|---|---|
| `context.hpp` | `ExecutionContext` — optional thread pool for parallel ops |
| `relation.hpp` | `Relation<T>` — immutable sorted/dedup set; `TupleLike` concept |
| `variable.hpp` | `Variable<T>` — semi-naive delta (stable / recent / to\_add) |
| `iteration.hpp` | `Iteration<T>` — fixpoint loop manager |
| `join.hpp` | `join_into`, `join_anti`, `gallop_lower_bound`, `find_key_range` |
| `extend.hpp` | `Leaper<T,V>`, `ExtendWith`, `FilterAnti`, `ExtendAnti`, `extend_into` |
| `aggregate.hpp` | `aggregate()` — parallel group-by fold |
| `index.hpp` | `SecondaryIndex<T>` — point + range queries |

---

## Blackboard

Shared between all three systems. Thread-safe, type-erased, with scoped overrides.

```cpp
Blackboard bb;

bb.set("health", 100);
auto hp = bb.get<int>("health");        // std::optional<int>

{
    auto scope = bb.pushScope();
    bb.set("health", 50);               // visible only in this scope
}                                       // automatically restored on exit
// bb.get<int>("health") == 100 again
```

---

## Building

```bash
make config     # configure (cmake, preserves cache)
make build      # compile everything
make test       # run all tests
make test TEST=test_logic   # run a specific test
```

**Requirements:** C++20 (GCC 10+, Clang 12+), CMake 3.15+

---

## License

MIT
