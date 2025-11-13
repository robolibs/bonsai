#include "bonsai/tree/builder.hpp"
#include "bonsai/tree/tree.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace bonsai::tree;

static Status fast_success(Blackboard &) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return Status::Success;
}

static Status slow_failure(Blackboard &) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return Status::Failure;
}

int main() {
    // Build a parallel tree that succeeds when any child succeeds (RequireOne)
    Builder b;
    b.parallel(Parallel::Policy::RequireOne, Parallel::Policy::RequireOne)
        .action(slow_failure)
        .action(slow_failure)
        .action(fast_success)
        .end();

    Tree t = b.build();

    auto t0 = std::chrono::steady_clock::now();
    auto status = t.tick();
    while (status == Status::Running) {
        status = t.tick();
    }
    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "Parallel finished with status: "
              << (status == Status::Success   ? "Success"
                  : status == Status::Failure ? "Failure"
                                              : "Running")
              << ", elapsed: " << ms << " ms\n";
    return 0;
}
