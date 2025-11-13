#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace bonsai::core {

    template <typename T> class task {
      public:
        struct promise_type {
            std::optional<T> value;
            std::exception_ptr exception;

            task get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void unhandled_exception() { exception = std::current_exception(); }
            template <typename U> void return_value(U &&v) { value = std::forward<U>(v); }
        };

        task() noexcept = default;
        explicit task(std::coroutine_handle<promise_type> h) : handle_(h) {}
        task(const task &) = delete;
        task &operator=(const task &) = delete;
        task(task &&other) noexcept : handle_(other.handle_) { other.handle_ = {}; }
        task &operator=(task &&other) noexcept {
            if (this != &other) {
                if (handle_)
                    handle_.destroy();
                handle_ = other.handle_;
                other.handle_ = {};
            }
            return *this;
        }
        ~task() {
            if (handle_)
                handle_.destroy();
        }

        bool done() const { return !handle_ || handle_.done(); }

        void resume() {
            if (handle_ && !handle_.done()) {
                handle_.resume();
            }
        }

        T result() {
            if (!handle_)
                throw std::runtime_error("task: no coroutine");
            if (!handle_.done())
                handle_.resume();
            if (handle_.promise().exception)
                std::rethrow_exception(handle_.promise().exception);
            T out = std::move(*handle_.promise().value);
            return out;
        }

        // Awaitable support
        bool await_ready() const noexcept { return done(); }
        void await_suspend(std::coroutine_handle<>) noexcept { resume(); }
        T await_resume() { return result(); }

      private:
        std::coroutine_handle<promise_type> handle_{};
    };

} // namespace bonsai::core
