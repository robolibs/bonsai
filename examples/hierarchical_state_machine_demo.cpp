#include <bonsai/bonsai.hpp>
#include <chrono>
#include <iostream>
#include <thread>

using namespace bonsai;

/**
 * Hierarchical State Machine Example: Music Player
 *
 * This demonstrates a complex music player with nested states:
 *
 * Top Level States:
 * - OFF
 * - ON (Composite state with substates)
 *
 * ON Substates:
 * - STOPPED
 * - PLAYING (Composite state with play modes)
 * - PAUSED
 *
 * PLAYING Substates:
 * - NORMAL_PLAY
 * - REPEAT_ONE
 * - REPEAT_ALL
 * - SHUFFLE
 */

class MusicPlayerDemo {
  public:
    void run() {
        std::cout << "\n=== Hierarchical State Machine Demo: Music Player ===\n" << std::endl;

        auto machine = buildMusicPlayer();

        // Simulate user interactions
        simulateUserInteractions(machine.get());
    }

  private:
    std::unique_ptr<state::StateMachine> buildMusicPlayer() {
        return state::Builder()
            .initial("OFF")

            // OFF state - simple state
            .state("OFF")
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🔌 Player OFF" << std::endl;
                bb.set("power", false);
            })
            .transitionTo(
                "ON", [](tree::Blackboard &bb) { return bb.get<std::string>("command").value_or("") == "power_on"; })

            // ON state - composite state with history
            .compositeState(
                "ON", state::CompositeState::HistoryType::Shallow,
                [](state::Builder &b) {
                    b.initial("STOPPED")

                        // STOPPED substate
                        .state("STOPPED")
                        .onEnter([](tree::Blackboard &bb) {
                            std::cout << "  ⏹️ Stopped" << std::endl;
                            bb.set("playback_state", std::string("stopped"));
                        })
                        .transitionTo(
                            "PLAYING",
                            [](tree::Blackboard &bb) { return bb.get<std::string>("command").value_or("") == "play"; })

                        // PLAYING substate - also composite!
                        .compositeState(
                            "PLAYING", state::CompositeState::HistoryType::Deep,
                            [](state::Builder &b) {
                                b.initial("NORMAL_PLAY")

                                    .state("NORMAL_PLAY")
                                    .onEnter([](tree::Blackboard &bb) {
                                        std::cout << "    ▶️ Playing (Normal Mode)" << std::endl;
                                        bb.set("play_mode", std::string("normal"));
                                    })
                                    .onUpdate([](tree::Blackboard &bb) {
                                        // Simulate playback progress
                                        int progress = bb.get<int>("progress").value_or(0);
                                        if (progress < 100) {
                                            bb.set("progress", progress + 10);
                                            std::cout << "      Progress: " << progress + 10 << "%" << std::endl;
                                        }
                                    })
                                    .transitionTo("REPEAT_ONE",
                                                  [](tree::Blackboard &bb) {
                                                      return bb.get<std::string>("mode_command").value_or("") ==
                                                             "repeat_one";
                                                  })
                                    .transitionTo("SHUFFLE",
                                                  [](tree::Blackboard &bb) {
                                                      return bb.get<std::string>("mode_command").value_or("") ==
                                                             "shuffle";
                                                  })

                                    .state("REPEAT_ONE")
                                    .onEnter([](tree::Blackboard &bb) {
                                        std::cout << "    🔂 Playing (Repeat One)" << std::endl;
                                        bb.set("play_mode", std::string("repeat_one"));
                                    })
                                    .onUpdate([](tree::Blackboard &bb) {
                                        int progress = bb.get<int>("progress").value_or(0);
                                        if (progress >= 100) {
                                            std::cout << "      Repeating current song..." << std::endl;
                                            bb.set("progress", 0);
                                        } else {
                                            bb.set("progress", progress + 10);
                                            std::cout << "      Progress: " << progress + 10 << "%" << std::endl;
                                        }
                                    })
                                    .transitionTo("REPEAT_ALL",
                                                  [](tree::Blackboard &bb) {
                                                      return bb.get<std::string>("mode_command").value_or("") ==
                                                             "repeat_all";
                                                  })
                                    .transitionTo("NORMAL_PLAY",
                                                  [](tree::Blackboard &bb) {
                                                      return bb.get<std::string>("mode_command").value_or("") ==
                                                             "normal";
                                                  })

                                    .state("REPEAT_ALL")
                                    .onEnter([](tree::Blackboard &bb) {
                                        std::cout << "    🔁 Playing (Repeat All)" << std::endl;
                                        bb.set("play_mode", std::string("repeat_all"));
                                    })
                                    .transitionTo("NORMAL_PLAY",
                                                  [](tree::Blackboard &bb) {
                                                      return bb.get<std::string>("mode_command").value_or("") ==
                                                             "normal";
                                                  })

                                    .state("SHUFFLE")
                                    .onEnter([](tree::Blackboard &bb) {
                                        std::cout << "    🔀 Playing (Shuffle Mode)" << std::endl;
                                        bb.set("play_mode", std::string("shuffle"));
                                    })
                                    .onUpdate([](tree::Blackboard &bb) {
                                        // Simulate random song selection
                                        static int songNum = 1;
                                        int progress = bb.get<int>("progress").value_or(0);
                                        if (progress >= 100) {
                                            songNum = (rand() % 5) + 1;
                                            std::cout << "      Shuffling to song #" << songNum << std::endl;
                                            bb.set("progress", 0);
                                        } else {
                                            bb.set("progress", progress + 15);
                                            std::cout << "      Song #" << songNum << " - Progress: " << progress + 15
                                                      << "%" << std::endl;
                                        }
                                    })
                                    .transitionTo("NORMAL_PLAY", [](tree::Blackboard &bb) {
                                        return bb.get<std::string>("mode_command").value_or("") == "normal";
                                    });
                            })
                        .onEnter([](tree::Blackboard &bb) {
                            std::cout << "  ▶️ Entering PLAYING state" << std::endl;
                            bb.set("playback_state", std::string("playing"));
                            bb.set("progress", 0);
                        })
                        .onExit([](tree::Blackboard &bb) { std::cout << "  ⏸️ Exiting PLAYING state" << std::endl; })
                        // Entry points for direct mode access
                        .entryPoint("play_shuffle", "SHUFFLE")
                        .entryPoint("play_repeat", "REPEAT_ONE")
                        // Transitions from PLAYING composite state
                        .transitionTo(
                            "PAUSED",
                            [](tree::Blackboard &bb) { return bb.get<std::string>("command").value_or("") == "pause"; })
                        .transitionTo(
                            "STOPPED",
                            [](tree::Blackboard &bb) { return bb.get<std::string>("command").value_or("") == "stop"; })

                        // PAUSED substate
                        .state("PAUSED")
                        .onEnter([](tree::Blackboard &bb) {
                            std::cout << "  ⏸️ Paused" << std::endl;
                            bb.set("playback_state", std::string("paused"));
                        })
                        .transitionTo(
                            "PLAYING",
                            [](tree::Blackboard &bb) { return bb.get<std::string>("command").value_or("") == "play"; })
                        .transitionTo("STOPPED", [](tree::Blackboard &bb) {
                            return bb.get<std::string>("command").value_or("") == "stop";
                        });
                })
            .onEnter([](tree::Blackboard &bb) {
                std::cout << "🔊 Player ON" << std::endl;
                bb.set("power", true);
            })
            .onExit([](tree::Blackboard &bb) { std::cout << "🔇 Powering down..." << std::endl; })
            .transitionTo(
                "OFF", [](tree::Blackboard &bb) { return bb.get<std::string>("command").value_or("") == "power_off"; })

            .build();
    }

    void simulateUserInteractions(state::StateMachine *machine) {
        auto &bb = machine->blackboard();

        // Helper to send command and tick
        auto sendCommand = [&](const std::string &cmd, int ticks = 1) {
            bb.set("command", cmd);
            for (int i = 0; i < ticks; ++i) {
                machine->tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            bb.set("command", std::string("")); // Clear command
        };

        auto sendModeCommand = [&](const std::string &cmd) {
            bb.set("mode_command", cmd);
            machine->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            bb.set("mode_command", std::string("")); // Clear command
        };

        // Scenario 1: Basic playback
        std::cout << "\n--- Scenario 1: Basic Playback ---\n" << std::endl;
        sendCommand("power_on");
        sendCommand("play");
        machine->tick(); // Let it play a bit
        machine->tick();
        sendCommand("pause");
        sendCommand("play");
        sendCommand("stop");

        // Scenario 2: Play mode changes
        std::cout << "\n--- Scenario 2: Play Mode Changes ---\n" << std::endl;
        sendCommand("play");
        sendModeCommand("repeat_one");
        machine->tick();
        machine->tick();
        sendModeCommand("shuffle");
        machine->tick();
        machine->tick();
        machine->tick();
        sendModeCommand("normal");

        // Scenario 3: History preservation
        std::cout << "\n--- Scenario 3: History Preservation ---\n" << std::endl;
        std::cout << "Setting shuffle mode and then power cycling..." << std::endl;
        sendModeCommand("shuffle");
        machine->tick();
        sendCommand("power_off");
        std::cout << "Powering back on (should restore to last state due to shallow history)..." << std::endl;
        sendCommand("power_on");
        machine->tick();

        // Show that we're back in playing state but lost the shuffle mode (shallow history)
        // For deep history, it would remember shuffle mode too

        // Scenario 4: Direct entry points
        std::cout << "\n--- Scenario 4: Entry Points (if implemented) ---\n" << std::endl;
        auto compositeOn = std::dynamic_pointer_cast<state::CompositeState>(machine->getCurrentState());
        if (compositeOn) {
            std::cout << "Current qualified state: " << compositeOn->getQualifiedStateName() << std::endl;
            std::cout << "Current substate: " << compositeOn->getCurrentSubstate() << std::endl;
        }

        // Final state
        sendCommand("power_off");

        std::cout << "\n✅ Hierarchical state machine demo completed!\n" << std::endl;
    }
};

/**
 * Example 2: Game Character with Nested Combat States
 */
class GameCharacterHSM {
  public:
    void run() {
        std::cout << "\n=== Game Character HSM Demo ===\n" << std::endl;

        auto character =
            state::Builder()
                .initial("IDLE")

                .state("IDLE")
                .onEnter([](tree::Blackboard &bb) { std::cout << "😴 Character idle" << std::endl; })
                .transitionTo("COMBAT",
                              [](tree::Blackboard &bb) { return bb.get<bool>("enemy_detected").value_or(false); })

                // COMBAT - composite state with complex substates
                .compositeState(
                    "COMBAT", state::CompositeState::HistoryType::Deep,
                    [](state::Builder &b) {
                        b.initial("ENGAGING")

                            // ENGAGING - another composite for different combat styles
                            .compositeState("ENGAGING", state::CompositeState::HistoryType::None,
                                            [](state::Builder &b) {
                                                b.initial("MELEE")

                                                    .state("MELEE")
                                                    .onEnter([](tree::Blackboard &bb) {
                                                        std::cout << "    ⚔️ Melee combat!" << std::endl;
                                                    })
                                                    .transitionTo("RANGED",
                                                                  [](tree::Blackboard &bb) {
                                                                      return bb.get<int>("distance").value_or(0) > 5;
                                                                  })

                                                    .state("RANGED")
                                                    .onEnter([](tree::Blackboard &bb) {
                                                        std::cout << "    🏹 Ranged combat!" << std::endl;
                                                    })
                                                    .transitionTo("MELEE",
                                                                  [](tree::Blackboard &bb) {
                                                                      return bb.get<int>("distance").value_or(10) <= 5;
                                                                  })

                                                    .state("MAGIC")
                                                    .onEnter([](tree::Blackboard &bb) {
                                                        std::cout << "    ✨ Magic combat!" << std::endl;
                                                    })
                                                    .onUpdate([](tree::Blackboard &bb) {
                                                        int mana = bb.get<int>("mana").value_or(100);
                                                        bb.set("mana", mana - 10);
                                                    })
                                                    .transitionTo("MELEE", [](tree::Blackboard &bb) {
                                                        return bb.get<int>("mana").value_or(0) <= 0;
                                                    });
                                            })
                            .transitionTo("DEFENSIVE",
                                          [](tree::Blackboard &bb) { return bb.get<int>("health").value_or(100) < 30; })

                            .state("DEFENSIVE")
                            .onEnter([](tree::Blackboard &bb) { std::cout << "  🛡️ Defensive stance!" << std::endl; })
                            .transitionTo("FLEEING",
                                          [](tree::Blackboard &bb) { return bb.get<int>("health").value_or(100) < 10; })
                            .transitionTo("ENGAGING",
                                          [](tree::Blackboard &bb) { return bb.get<int>("health").value_or(0) > 50; })

                            .state("FLEEING")
                            .onEnter([](tree::Blackboard &bb) { std::cout << "  🏃 Fleeing!" << std::endl; });
                    })
                .onEnter([](tree::Blackboard &bb) { std::cout << "⚔️ Entering COMBAT mode" << std::endl; })
                .transitionTo("IDLE",
                              [](tree::Blackboard &bb) { return !bb.get<bool>("enemy_detected").value_or(true); })

                .build();

        // Simulate combat scenario
        auto &bb = character->blackboard();

        std::cout << "\nDetecting enemy..." << std::endl;
        bb.set("enemy_detected", true);
        bb.set("distance", 3);
        bb.set("health", 100);
        bb.set("mana", 100);
        character->tick();

        std::cout << "\nEnemy moves away..." << std::endl;
        bb.set("distance", 10);
        character->tick();

        std::cout << "\nUsing magic..." << std::endl;
        bb.set("distance", 15);
        for (int i = 0; i < 5; ++i) {
            character->tick();
        }

        std::cout << "\nTaking damage..." << std::endl;
        bb.set("health", 25);
        character->tick();

        std::cout << "\nCritical health..." << std::endl;
        bb.set("health", 5);
        character->tick();

        std::cout << "\nEnemy defeated..." << std::endl;
        bb.set("enemy_detected", false);
        character->tick();

        std::cout << "\n✅ Game character HSM demo completed!\n" << std::endl;
    }
};

int main() {
    try {
        MusicPlayerDemo musicPlayer;
        musicPlayer.run();

        GameCharacterHSM gameCharacter;
        gameCharacter.run();

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}