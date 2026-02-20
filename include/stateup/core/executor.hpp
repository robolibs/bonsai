#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace stateup::core {

    class ThreadPool {
      public:
        explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) : stop_(false) {
            if (threads == 0)
                threads = 1;
            for (size_t i = 0; i < threads; ++i) {
                workers_.emplace_back([this] {
                    for (;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lk(m_);
                            cv_.wait(lk, [&] { return stop_ || !q_.empty(); });
                            if (stop_ && q_.empty())
                                return;
                            task = std::move(q_.front());
                            q_.pop();
                        }
                        task();
                    }
                });
            }
        }
        ~ThreadPool() {
            {
                std::lock_guard<std::mutex> lk(m_);
                stop_ = true;
            }
            cv_.notify_all();
            for (auto &t : workers_)
                t.join();
        }

        template <class F, class... A> auto submit(F &&f, A &&...a) -> std::future<decltype(f(a...))> {
            using R = decltype(f(a...));
            auto p = std::make_shared<std::packaged_task<R()>>(std::bind(std::forward<F>(f), std::forward<A>(a)...));
            {
                std::lock_guard<std::mutex> lk(m_);
                q_.emplace([p] { (*p)(); });
            }
            cv_.notify_one();
            return p->get_future();
        }

        template <class F> void bulk(F &&f, size_t n) {
            std::promise<void> done;
            std::atomic<size_t> remaining{n};
            auto fut = done.get_future();
            for (size_t i = 0; i < n; ++i) {
                submit([&, i] {
                    f(i);
                    if (remaining.fetch_sub(1) == 1)
                        done.set_value();
                });
            }
            fut.get();
        }

        // Early-stop bulk: f(i) returns true if work should continue, false to signal stop
        template <class F> void bulk_early_stop(F &&f, size_t n, std::atomic<bool> &stop) {
            std::promise<void> done;
            std::atomic<size_t> remaining{n};
            auto fut = done.get_future();
            for (size_t i = 0; i < n; ++i) {
                submit([&, i] {
                    if (!stop.load(std::memory_order_relaxed)) {
                        bool cont = f(i);
                        if (!cont) {
                            stop.store(true, std::memory_order_relaxed);
                        }
                    }
                    if (remaining.fetch_sub(1) == 1)
                        done.set_value();
                });
            }
            fut.get();
        }

      private:
        std::mutex m_;
        std::condition_variable cv_;
        std::queue<std::function<void()>> q_;
        std::vector<std::thread> workers_;
        bool stop_;
    };

} // namespace stateup::core
