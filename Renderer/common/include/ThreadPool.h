#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <tuple>
#include <type_traits>
#include "Logger.h"
#include "TaskPriority.h"
#include "PriorityTaskQueue.h"

namespace Async {

class ThreadPool {
public:

    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Disable copy and move operations
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    template<class F, class... Args>
    auto enqueue(Async::TaskPriority priority, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    template<class F, class... Args>
    void enqueue_detached(F&& f, Args&&... args);


    template<class F, class... Args>
    void enqueue_detached(Async::TaskPriority priority, F&& f, Args&&... args);

    // Convenience methods for specific priorities
    template<class F, class... Args>
    auto enqueue_background(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        return enqueue(Async::TaskPriority::k_background, std::forward<F>(f), std::forward<Args>(args)...);
    }

    template<class F, class... Args>
    auto enqueue_high(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        return enqueue(Async::TaskPriority::k_high, std::forward<F>(f), std::forward<Args>(args)...);
    }

    template<class F, class... Args>
    auto enqueue_critical(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        return enqueue(Async::TaskPriority::k_critical, std::forward<F>(f), std::forward<Args>(args)...);
    }


    size_t get_thread_count() const;

    size_t get_pending_task_count() const;

    size_t getActiveTaskCount() const;

    bool isRunning() const;

    void stop(bool waitForCompletion = true);

    void waitForAll();

    struct Statistics {
        size_t totalThreads;
        size_t activeThreads;
        size_t pendingTasks;
        size_t completedTasks;
        bool isRunning;
        
        // Priority-specific statistics
        struct PriorityStats {
            size_t backgroundTasks;
            size_t normalTasks;
            size_t highTasks;
            size_t criticalTasks;
        } priorityStats;
    };

    Statistics getStatistics() const;

protected:
    // Worker thread function
    virtual void workerThread();
    
    // Helper to wrap move-only callables
    template<typename F>
    static std::function<void()> makeTask(F&& f) {
        return std::forward<F>(f);
    }

    // Thread management
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    std::atomic<size_t> activeThreads_;
    std::atomic<size_t> completedTasks_;

    // Priority task queue 
    PriorityTaskQueue priority_queue_;
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::condition_variable finishedCondition_;

    // Thread pool configuration
    const size_t numThreads_;
};

// Template implementation
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    return enqueue(Async::TaskPriority::k_normal, std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F, class... Args>
auto ThreadPool::enqueue(Async::TaskPriority priority, F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    
    using return_type = typename std::invoke_result<F, Args...>::type;

    if (stop_.load()) {
        throw std::runtime_error("ThreadPool: Cannot enqueue task - thread pool is stopped");
    }

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    // Submit to priority queue (no need for additional mutex since PriorityTaskQueue is thread-safe)
    uint64_t task_id = priority_queue_.submit([task](){ (*task)(); }, priority);
    
    LOG_DEBUG("ThreadPool: Task enqueued with priority {} ({}), task ID: {}, queue size: {}", 
              static_cast<int>(priority), Async::priority_to_string(priority), task_id, priority_queue_.size());
    
    condition_.notify_one();
    return result;
}

template<class F, class... Args>
void ThreadPool::enqueue_detached(F&& f, Args&&... args) {
    enqueue_detached(Async::TaskPriority::k_normal, std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F, class... Args>
void ThreadPool::enqueue_detached(Async::TaskPriority priority, F&& f, Args&&... args) {
    if (stop_.load()) {
        LOG_WARN("ThreadPool: Cannot enqueue detached task - thread pool is stopped");
        return;
    }

    // Use auto instead of std::function to avoid copy requirements
    auto boundTask = [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        try {
            std::apply(std::move(f), std::move(args));
        } catch (const std::exception& e) {
            LOG_ERROR("ThreadPool: Exception in detached task: {}", e.what());
        }
    };

    // Wrap in shared_ptr for type erasure
    auto taskPtr = std::make_shared<decltype(boundTask)>(std::move(boundTask));

    // Submit to priority queue
    uint64_t task_id = priority_queue_.submit([taskPtr]() { (*taskPtr)(); }, priority);
    
    LOG_DEBUG("ThreadPool: Detached task enqueued with priority {} ({}), task ID: {}, queue size: {}", 
              static_cast<int>(priority), Async::priority_to_string(priority), task_id, priority_queue_.size());
    
    condition_.notify_one();
}

} // namespace Async

