#include <bonsai/bonsai.hpp>
#include <doctest/doctest.h>
#include <string>
#include <thread>
#include <vector>

using namespace bonsai::tree;

TEST_CASE("Blackboard basic operations") {
    Blackboard bb;

    SUBCASE("Set and get integer") {
        bb.set("test_int", 42);
        auto value = bb.get<int>("test_int");
        REQUIRE(value.has_value());
        CHECK(value.value() == 42);
    }

    SUBCASE("Set and get string") {
        bb.set("test_string", std::string("hello"));
        auto value = bb.get<std::string>("test_string");
        REQUIRE(value.has_value());
        CHECK(value.value() == "hello");
    }

    SUBCASE("Set and get double") {
        bb.set("test_double", 3.14159);
        auto value = bb.get<double>("test_double");
        REQUIRE(value.has_value());
        CHECK(value.value() == doctest::Approx(3.14159));
    }

    SUBCASE("Get non-existent key") {
        auto value = bb.get<int>("non_existent");
        CHECK_FALSE(value.has_value());
    }

    SUBCASE("Type mismatch") {
        bb.set("test_int", 42);
        auto value = bb.get<std::string>("test_int");
        CHECK_FALSE(value.has_value());
    }
}

TEST_CASE("Blackboard has and remove operations") {
    Blackboard bb;

    SUBCASE("Has operation") {
        CHECK_FALSE(bb.has("test_key"));
        bb.set("test_key", 123);
        CHECK(bb.has("test_key"));
    }

    SUBCASE("Remove operation") {
        bb.set("test_key", 456);
        CHECK(bb.has("test_key"));
        bb.remove("test_key");
        CHECK_FALSE(bb.has("test_key"));
    }

    SUBCASE("Clear operation") {
        bb.set("key1", 1);
        bb.set("key2", 2);
        bb.set("key3", 3);

        CHECK(bb.has("key1"));
        CHECK(bb.has("key2"));
        CHECK(bb.has("key3"));

        bb.clear();

        CHECK_FALSE(bb.has("key1"));
        CHECK_FALSE(bb.has("key2"));
        CHECK_FALSE(bb.has("key3"));
    }
}

TEST_CASE("Blackboard thread safety") {
    Blackboard bb;
    const int num_threads = 10;
    const int operations_per_thread = 100;

    std::vector<std::thread> threads;

    // Create threads that concurrently set values
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&bb, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string key = "thread_" + std::to_string(i) + "_key_" + std::to_string(j);
                int value = i * 1000 + j;
                bb.set(key, value);

                // Verify we can read back what we wrote
                auto retrieved = bb.get<int>(key);
                CHECK(retrieved.has_value());
                if (retrieved.has_value()) {
                    CHECK(retrieved.value() == value);
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto &thread : threads) {
        thread.join();
    }

    // Verify all values are present
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < operations_per_thread; ++j) {
            std::string key = "thread_" + std::to_string(i) + "_key_" + std::to_string(j);
            int expected_value = i * 1000 + j;
            auto value = bb.get<int>(key);
            REQUIRE(value.has_value());
            CHECK(value.value() == expected_value);
        }
    }
}

TEST_CASE("Blackboard scopes") {
    Blackboard bb;
    bb.set("shared", 1);

    {
        auto scope = bb.pushScope();
        bb.set("shared", 2);
        bb.set("inner_only", 99);

        auto shared = bb.get<int>("shared");
        REQUIRE(shared.has_value());
        CHECK(shared.value() == 2);

        scope.release();
    }

    auto shared_after = bb.get<int>("shared");
    REQUIRE(shared_after.has_value());
    CHECK(shared_after.value() == 1);

    auto inner_value = bb.get<int>("inner_only");
    CHECK_FALSE(inner_value.has_value());
}

TEST_CASE("Blackboard observer notifications") {
    Blackboard bb;
    std::vector<Blackboard::Event> events;
    bb.setObserver([&events](const Blackboard::Event &event) { events.push_back(event); });

    bb.set("key", 42);
    auto value = bb.get<int>("key");
    (void)value;
    bb.get<int>("missing");
    bb.remove("key");
    bb.clear();

    REQUIRE(events.size() >= 4);
    CHECK(events[0].type == Blackboard::Event::Type::Set);
    CHECK(events[1].type == Blackboard::Event::Type::Get);
    CHECK(events[1].success);
    CHECK(events[2].type == Blackboard::Event::Type::Get);
    CHECK_FALSE(events[2].success);
}
