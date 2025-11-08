#include <bonsai/bonsai.hpp>
#include <iostream>

using namespace bonsai;

// Example demonstrating Guard/Entry/Exit lifecycle
void doorLockExample() {
    std::cout << "\n=== Door Lock State Machine with Guard ===" << std::endl;

    auto machine =
        state::Builder()
            .initial("locked")
            .state("locked")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "  [ENTER] Door is now LOCKED" << std::endl;
                bb.set<bool>("is_locked", true);
            })
            .onUpdate([](tree::Blackboard &bb) {
                std::cout << "  [UPDATE] Checking for unlock request..." << std::endl;
                // Simulate receiving correct PIN
                static int tick = 0;
                tick++;
                if (tick == 2) {
                    std::cout << "  -> PIN entered: 1234" << std::endl;
                    bb.set<std::string>("pin", "1234");
                }
            })
            .transitionTo("unlocked",
                          [](tree::Blackboard &bb) {
                              return bb.has("pin"); // Try to unlock when PIN is entered
                          })
            .onExit([](tree::Blackboard &bb) { std::cout << "  [EXIT] Leaving locked state..." << std::endl; })
            .state("unlocked")
            .onGuard([](tree::Blackboard &bb) {
                // Guard: Only unlock if PIN is correct
                auto pin = bb.get<std::string>("pin").value_or("");
                bool correct = (pin == "1234");
                std::cout << "  [GUARD] Checking PIN '" << pin << "': " << (correct ? "CORRECT ✓" : "WRONG ✗")
                          << std::endl;
                return correct;
            })
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "  [ENTER] Door is now UNLOCKED" << std::endl;
                bb.set<bool>("is_locked", false);
            })
            .onUpdate([](tree::Blackboard &bb) { std::cout << "  [UPDATE] Door unlocked" << std::endl; })
            .build();

    std::cout << "\nTick 1:" << std::endl;
    machine->tick();
    std::cout << "State: " << machine->getCurrentStateName() << std::endl;

    std::cout << "\nTick 2:" << std::endl;
    machine->tick();
    std::cout << "State: " << machine->getCurrentStateName() << std::endl;

    std::cout << "\nTick 3:" << std::endl;
    machine->tick();
    std::cout << "State: " << machine->getCurrentStateName() << std::endl;
}

// Example showing guard blocking invalid transitions
void atmWithdrawalExample() {
    std::cout << "\n=== ATM Withdrawal with Guard Conditions ===" << std::endl;

    auto machine =
        state::Builder()
            .initial("idle")
            .state("idle")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "  [ENTER] ATM ready" << std::endl;
                bb.set<int>("balance", 1000);
                bb.set<int>("withdrawal_amount", 0);
            })
            .onUpdate([](tree::Blackboard &bb) {
                static int tick = 0;
                tick++;
                if (tick == 1) {
                    std::cout << "  [UPDATE] Customer requests $500 withdrawal" << std::endl;
                    bb.set<int>("withdrawal_amount", 500);
                } else if (tick == 2) {
                    std::cout << "  [UPDATE] Customer requests $1500 withdrawal" << std::endl;
                    bb.set<int>("withdrawal_amount", 1500);
                }
            })
            .transitionTo("dispensing",
                          [](tree::Blackboard &bb) { return bb.get<int>("withdrawal_amount").value_or(0) > 0; })
            .state("dispensing")
            .onGuard([](tree::Blackboard &bb) {
                // Guard: Only allow if sufficient balance
                int balance = bb.get<int>("balance").value_or(0);
                int amount = bb.get<int>("withdrawal_amount").value_or(0);
                bool allowed = (amount <= balance);
                std::cout << "  [GUARD] Check withdrawal $" << amount << " against balance $" << balance << ": "
                          << (allowed ? "APPROVED ✓" : "DENIED ✗") << std::endl;
                return allowed;
            })
            .onEnter([](tree::Blackboard &bb) {
                int amount = bb.get<int>("withdrawal_amount").value_or(0);
                int balance = bb.get<int>("balance").value_or(0);
                bb.set<int>("balance", balance - amount);
                std::cout << "  [ENTER] Dispensing $" << amount << ", new balance: $" << (balance - amount)
                          << std::endl;
            })
            .onExit([](tree::Blackboard &bb) {
                std::cout << "  [EXIT] Transaction complete" << std::endl;
                bb.set<int>("withdrawal_amount", 0);
            })
            .transitionTo("idle", [](tree::Blackboard &bb) { return true; })
            .build();

    std::cout << "\nTick 1 (Enter idle):" << std::endl;
    machine->tick();

    std::cout << "\nTick 2 (Request $500 - should succeed):" << std::endl;
    machine->tick();

    std::cout << "\nTick 3 (Return to idle):" << std::endl;
    machine->tick();

    std::cout << "\nTick 4 (Request $1500 - should fail guard):" << std::endl;
    machine->tick();

    std::cout << "\nFinal balance: $" << machine->blackboard().get<int>("balance").value_or(0) << std::endl;
}

// Example showing entry/exit with resource management
void resourceManagerExample() {
    std::cout << "\n=== Resource Manager with Entry/Exit ===" << std::endl;

    auto machine =
        state::Builder()
            .initial("idle")
            .state("idle")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "  [ENTER] System idle - resources released" << std::endl;
                bb.set<bool>("resource_acquired", false);
            })
            .transitionTo("working", [](tree::Blackboard &bb) { return true; })
            .state("working")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "  [ENTER] Acquiring resources (opening files, allocating memory)..." << std::endl;
                bb.set<bool>("resource_acquired", true);
                bb.set<int>("work_done", 0);
            })
            .onUpdate([](tree::Blackboard &bb) {
                int work = bb.get<int>("work_done").value_or(0);
                work++;
                bb.set<int>("work_done", work);
                std::cout << "  [UPDATE] Doing work... (" << work << "/3 units)" << std::endl;
            })
            .onExit([](tree::Blackboard &bb) {
                std::cout << "  [EXIT] Releasing resources (closing files, freeing memory)..." << std::endl;
                bb.set<bool>("resource_acquired", false);
            })
            .transitionTo("idle", [](tree::Blackboard &bb) { return bb.get<int>("work_done").value_or(0) >= 3; })
            .build();

    std::cout << "\nTick 1:" << std::endl;
    machine->tick();

    std::cout << "\nTick 2:" << std::endl;
    machine->tick();

    std::cout << "\nTick 3:" << std::endl;
    machine->tick();

    std::cout << "\nTick 4:" << std::endl;
    machine->tick();

    std::cout << "\nTick 5 (work complete, cleanup):" << std::endl;
    machine->tick();

    bool resources = machine->blackboard().get<bool>("resource_acquired").value_or(true);
    std::cout << "\nResources properly cleaned up: " << (resources ? "NO ✗" : "YES ✓") << std::endl;
}

int main() {
    std::cout << "Bonsai State Machine - Guard/Entry/Exit Lifecycle Demo" << std::endl;
    std::cout << "=======================================================" << std::endl;

    doorLockExample();
    atmWithdrawalExample();
    resourceManagerExample();

    std::cout << "\n✅ All lifecycle examples completed!" << std::endl;
    return 0;
}
