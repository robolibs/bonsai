#pragma once

#include "stateup/core/executor.hpp"

namespace stateup::logic {

    // Execution context for datalog evaluation.
    // Wraps an optional non-owning pointer to a ThreadPool.
    // If no pool is provided, all operations run single-threaded.
    // The caller is responsible for the ThreadPool's lifetime.
    class ExecutionContext {
      public:
        ExecutionContext() = default;

        explicit ExecutionContext(stateup::core::ThreadPool *pool) : pool_(pool) {}

        bool has_parallel() const { return pool_ != nullptr; }

        stateup::core::ThreadPool *pool() const { return pool_; }

      private:
        stateup::core::ThreadPool *pool_ = nullptr;
    };

} // namespace stateup::logic
