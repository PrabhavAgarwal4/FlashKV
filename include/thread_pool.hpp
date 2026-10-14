#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <stdexcept>

namespace flashkv {

using std::vector;
using std::queue;
using std::thread;
using std::mutex;
using std::unique_lock;
using std::condition_variable;
using std::function;
using std::future;
using std::packaged_task;
using std::make_shared;
using std::atomic;
using std::size_t;

/**
 * @brief ThreadPool implementing the Producer-Consumer pattern.
 * Pre-spawns a fixed set of worker threads that sleep when idle,
 * eliminating the costly overhead of thread creation and context switching.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = thread::hardware_concurrency()) 
        : stop_(false) {
        if (num_threads == 0) {
            num_threads = 1;
        }

        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    }

    // Prevent copying and moving (threads and mutexes are non-copyable)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() {
        shutdown();
    }

    /**
     * @brief Enqueues a task to be executed by one of the worker threads.
     * Returns a std::future containing the result of the function execution.
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        // Package the task into a shared pointer so it can be copied into the std::function wrapper
        auto task = make_shared<packaged_task<return_type()>>(
            [func = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
                return func(args...);
            }
        );

        future<return_type> res = task->get_future();
        {
            unique_lock<mutex> lock(queue_mutex_);
            if (stop_.load()) {
                throw std::runtime_error("Cannot submit task to stopped ThreadPool");
            }
            tasks_.emplace([task]() {
                (*task)();
            });
        }
        // Wake up exactly one sleeping worker
        cv_.notify_one();
        return res;
    }

    /**
     * @brief Current number of worker threads.
     */
    size_t worker_count() const {
        return workers_.size();
    }

    /**
     * @brief Number of tasks waiting in the queue.
     */
    size_t pending_tasks() {
        unique_lock<mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    /**
     * @brief Initiates graceful shutdown and waits for all threads to exit.
     */
    void shutdown() {
        {
            unique_lock<mutex> lock(queue_mutex_);
            if (stop_.load()) {
                return;
            }
            stop_.store(true);
        }
        // Wake up all sleeping workers so they can observe stop_ and exit
        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

private:
    void worker_loop() {
        while (true) {
            function<void()> task;
            {
                unique_lock<mutex> lock(queue_mutex_);
                // Sleep until a task is available or shutdown is requested (guards against spurious wakeups)
                cv_.wait(lock, [this] {
                    return stop_.load() || !tasks_.empty();
                });

                if (stop_.load() && tasks_.empty()) {
                    return; // All work complete, thread exits
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }
            // Execute task outside the lock so other workers can proceed in parallel
            task();
        }
    }

    vector<thread> workers_;
    queue<function<void()>> tasks_;
    mutex queue_mutex_;
    condition_variable cv_;
    atomic<bool> stop_;
};

} // namespace flashkv
