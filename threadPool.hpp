#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <queue>

class ThreadGuard {
    std::thread t_;
public:
    explicit ThreadGuard(std::thread t) : t_(std::move(t)) {}
    ~ThreadGuard() { if (t_.joinable()) t_.join(); }
    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
    ThreadGuard(ThreadGuard&&) noexcept = default;
};

// ─────────────────────────────────────────────────────────────────────────────
// ThreadPool: Simple thread pool to handle 100+ tasks
// ─────────────────────────────────────────────────────────────────────────────
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mtx_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto &worker : workers_) worker.join();
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mtx_;
    std::condition_variable condition_;
    bool stop_;
};
