#pragma once
#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace bonsai::core {

    // Thread-safe execution guard with recursion depth tracking
    class ExecutionGuard {
      public:
        static constexpr size_t MAX_RECURSION_DEPTH = 1000;

        ExecutionGuard() : depth_(0) {}

        bool enterRecursion() {
            size_t current = depth_.fetch_add(1, std::memory_order_relaxed);
            if (current >= MAX_RECURSION_DEPTH) {
                depth_.fetch_sub(1, std::memory_order_relaxed);
                return false;
            }
            return true;
        }

        void exitRecursion() { depth_.fetch_sub(1, std::memory_order_relaxed); }

        class ScopedRecursion {
          public:
            ScopedRecursion(ExecutionGuard &guard, bool &success) : guard_(guard), entered_(guard.enterRecursion()) {
                success = entered_;
            }
            ~ScopedRecursion() {
                if (entered_) {
                    guard_.exitRecursion();
                }
            }

          private:
            ExecutionGuard &guard_;
            bool entered_;
        };

      private:
        std::atomic<size_t> depth_;
    };

} // namespace bonsai::core