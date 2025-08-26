#pragma once

#include <coroutine>
#include <memory>
#include <queue>
#include <chrono>
#include <type_traits>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <optional>
#include <thread>

#include "EnhancedThreadPool.h"
#include "TaskPriority.h"
#include "Task.h"

namespace Async {

    // Forward declarations
    class WorkStealingQueue;
    class CoroutineThreadPoolScheduler;

    // Priority coroutine wrapper structure
    struct PriorityCoroutine {
        std::coroutine_handle<> handle;
        Async::TaskPriority priority;
        std::chrono::steady_clock::time_point submit_time;
        uint64_t task_id;
        
        PriorityCoroutine(std::coroutine_handle<> h, Async::TaskPriority p = Async::TaskPriority::k_normal, uint64_t id = 0)
            : handle(h), priority(p), submit_time(std::chrono::steady_clock::now()), task_id(id) {}
        
        // Move semantics
        PriorityCoroutine(PriorityCoroutine&& other) noexcept
            : handle(other.handle), priority(other.priority), submit_time(other.submit_time), task_id(other.task_id) {
            other.handle = nullptr;
        }
        
        PriorityCoroutine& operator=(PriorityCoroutine&& other) noexcept {
            if (this != &other) {
                handle = other.handle;
                priority = other.priority;
                submit_time = other.submit_time;
                task_id = other.task_id;
                other.handle = nullptr;
            }
            return *this;
        }
        
        // Disable copy
        PriorityCoroutine(const PriorityCoroutine&) = delete;
        PriorityCoroutine& operator=(const PriorityCoroutine&) = delete;
        
        bool operator<(const PriorityCoroutine& other) const {
            if (priority != other.priority) {
                return priority < other.priority;
            }
            return submit_time > other.submit_time;
        }
    };

    // Work stealing queue for coroutines
    class WorkStealingQueue {
    private:
        std::deque<PriorityCoroutine> tasks_;
        mutable std::mutex mutex_;
        std::atomic<size_t> approximate_size_{0};
        
    public:
        void push(PriorityCoroutine task) {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push_back(std::move(task));
            approximate_size_.fetch_add(1, std::memory_order_relaxed);
        }
        
        std::optional<PriorityCoroutine> pop() {
            std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
            if (!lock.owns_lock() || tasks_.empty()) {
                return std::nullopt;
            }
            
            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            approximate_size_.fetch_sub(1, std::memory_order_relaxed);
            return task;
        }
        
        std::optional<PriorityCoroutine> steal() {
            std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
            if (!lock.owns_lock() || tasks_.empty()) {
                return std::nullopt;
            }
            
            auto task = std::move(tasks_.back());
            tasks_.pop_back();
            approximate_size_.fetch_sub(1, std::memory_order_relaxed);
            return task;
        }
        
        size_t size() const {
            return approximate_size_.load(std::memory_order_relaxed);
        }
        
        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return tasks_.empty();
        }
    };

    // Context switch awaiter for switching between main thread and thread pool
    class ContextSwitchAwaiter {
    private:
        CoroutineThreadPoolScheduler* scheduler_;
        bool switch_to_main_;
        
    public:
        ContextSwitchAwaiter(CoroutineThreadPoolScheduler* scheduler, bool switch_to_main)
            : scheduler_(scheduler), switch_to_main_(switch_to_main) {}
        
        bool await_ready() const noexcept { return false; }
        
        void await_suspend(std::coroutine_handle<> handle);
        
        void await_resume() const noexcept {}
    };

    // Submit to thread pool awaiter
    template<typename F>
    class SubmitToThreadPoolAwaiter {
    private:
        F function_;
        CoroutineThreadPoolScheduler* scheduler_;
        using ResultType = std::invoke_result_t<F>;
        mutable std::optional<ResultType> result_;
        mutable std::exception_ptr exception_ptr_;
        
    public:
        SubmitToThreadPoolAwaiter(F&& func, CoroutineThreadPoolScheduler* scheduler)
          : function_(std::forward<F>(func)), scheduler_(scheduler) {}
        
        bool await_ready() const noexcept { return false; }
        
        void await_suspend(std::coroutine_handle<> handle);
        
        ResultType await_resume() {
            if (exception_ptr_) {
                std::rethrow_exception(exception_ptr_);
            }
            if constexpr (!std::is_void_v<ResultType>) {
                return std::move(result_.value());
            }
        }
        
        // Methods for setting result and exception
        void set_result(ResultType&& result) const requires(!std::is_void_v<ResultType>) {
            result_ = std::move(result);
        }
        
        void set_exception(std::exception_ptr ptr) const {
            exception_ptr_ = ptr;
        }
    };

    // Main coroutine thread pool scheduler class
    class CoroutineThreadPoolScheduler : public std::enable_shared_from_this<CoroutineThreadPoolScheduler> {
    public:
        // Statistics structure (non-atomic for return values)
        struct Stats {
            size_t coroutines_submitted{0};
            size_t coroutines_completed{0};
            size_t work_steals_attempted{0};
            size_t work_steals_successful{0};
            
            struct PriorityStats {
                size_t background_tasks{0};
                size_t normal_tasks{0};
                size_t high_tasks{0};
                size_t critical_tasks{0};
            } priority_stats;
        };
        
        // Internal statistics structure (atomic for thread safety)
        struct InternalStats {
            std::atomic<size_t> coroutines_submitted{0};
            std::atomic<size_t> coroutines_completed{0};
            std::atomic<size_t> work_steals_attempted{0};
            std::atomic<size_t> work_steals_successful{0};
            
            struct PriorityStats {
                std::atomic<size_t> background_tasks{0};
                std::atomic<size_t> normal_tasks{0};
                std::atomic<size_t> high_tasks{0};
                std::atomic<size_t> critical_tasks{0};
            } priority_stats;
        };

        // Constructors and destructor
        explicit CoroutineThreadPoolScheduler(size_t thread_count = std::thread::hardware_concurrency());
        ~CoroutineThreadPoolScheduler();

        // Disable copy
        CoroutineThreadPoolScheduler(const CoroutineThreadPoolScheduler&) = delete;
        CoroutineThreadPoolScheduler& operator=(const CoroutineThreadPoolScheduler&) = delete;

        // Initialization and lifecycle methods
        void initialize();
        void shutdown(bool wait_for_completion = true);
        bool is_running() const noexcept;

        // Core scheduling methods
        void schedule_coroutine(std::coroutine_handle<> handle, Async::TaskPriority priority = Async::TaskPriority::k_normal);
        
        // Thread pool integration
        bool try_process_coroutine_in_threadpool(size_t worker_index);
        
        // Main thread coroutine processing
        size_t process_main_thread_coroutines();
        
        // Submit functions to thread pool
        template<typename F>
        auto submit_to_threadpool(F&& func) -> SubmitToThreadPoolAwaiter<F> {
            return SubmitToThreadPoolAwaiter<F>(std::forward<F>(func), this);
        }
        
        // Submit functions to thread pool with priority (for compatibility)
        template<typename F>
        auto submit_to_threadpool(Async::TaskPriority priority, F&& func) -> SubmitToThreadPoolAwaiter<F> {
            // Priority could be handled here if needed in the future
            return SubmitToThreadPoolAwaiter<F>(std::forward<F>(func), this);
        }

        // Async file operations
        Task<std::vector<uint8_t>> ReadFileAsync(const std::string& filepath);
        
        // Context switching methods
        Task<void> SwitchToMain();
        Task<void> SwitchToThreadPool();

        // Statistics and monitoring
        Stats get_stats() const noexcept;
        void reset_stats() noexcept;

        // Global instance access
        static CoroutineThreadPoolScheduler& get_instance();

        // Friend declarations for awaiter access
        friend class ContextSwitchAwaiter;
        template<typename F>
        friend class SubmitToThreadPoolAwaiter;

    private:
        // Thread pool for CPU-intensive tasks
        std::unique_ptr<EnhancedThreadPool> thread_pool_;
        
        // Work stealing queues for each thread
        std::vector<std::unique_ptr<WorkStealingQueue>> worker_queues_;
        
        // Global priority queue for high-priority coroutines
        std::priority_queue<PriorityCoroutine> global_coroutine_queue_;
        mutable std::mutex global_coroutine_mutex_;
        
        // Main thread coroutine queue
        std::queue<std::coroutine_handle<>> main_thread_queue_;
        mutable std::mutex main_thread_mutex_;
        
        // Control flags
        std::atomic<bool> running_{true};
        std::thread::id main_thread_id_;
        
        // Statistics
        mutable std::mutex stats_mutex_;
        InternalStats stats_;
        
        // Synchronization
        std::condition_variable coroutine_available_;
        
        // Task ID generation
        std::atomic<uint64_t> next_coroutine_id_{1};
        
        // Thread local storage for worker index
        static thread_local size_t current_worker_index_;

        // Private helper methods
        std::optional<PriorityCoroutine> try_steal_coroutine_work(size_t current_worker);
        void distribute_coroutine_to_worker(PriorityCoroutine task);
        size_t get_least_loaded_coroutine_worker() const noexcept;
        
        // Internal implementation methods for awaiters
        void schedule_to_main_thread(std::coroutine_handle<> handle);
        void schedule_to_thread_pool(std::coroutine_handle<> handle);
        
        template<typename F>
        void execute_in_thread_pool(F&& func, std::coroutine_handle<> continuation, SubmitToThreadPoolAwaiter<F>* awaiter);
    };

    // Template implementations
    template<typename F>
    void SubmitToThreadPoolAwaiter<F>::await_suspend(std::coroutine_handle<> handle) {
        scheduler_->execute_in_thread_pool(std::move(function_), handle, this);
    }

        template<typename F>
    void CoroutineThreadPoolScheduler::execute_in_thread_pool(F&& func, std::coroutine_handle<> continuation, SubmitToThreadPoolAwaiter<F>* awaiter) {
        if (!thread_pool_) {
            // Thread pool not available - set exception
            Logger::get_instance().error("CoroutineThreadPoolScheduler: Thread pool not available in execute_in_thread_pool");
            awaiter->set_exception(std::make_exception_ptr(std::runtime_error("Thread pool not available")));
            // Schedule continuation to main thread instead of resuming directly
            schedule_to_main_thread(continuation);
            return;
        }
        
        if (!thread_pool_->isRunning()) {
            Logger::get_instance().error("CoroutineThreadPoolScheduler: Thread pool not running in execute_in_thread_pool");
            awaiter->set_exception(std::make_exception_ptr(std::runtime_error("Thread pool not running")));
            schedule_to_main_thread(continuation);
            return;
        }
        
        // Submit function to thread pool
        try {
            thread_pool_->enqueue([func = std::forward<F>(func), continuation, awaiter, this]() mutable {
                try {
                    if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                        func();
                    } else {
                        auto result = func();
                        awaiter->set_result(std::move(result));
                    }
                    // Schedule continuation to main thread
                    this->schedule_to_main_thread(continuation);
                } catch (...) {
                    awaiter->set_exception(std::current_exception());
                    this->schedule_to_main_thread(continuation);
                }
            });
        } catch (const std::exception& e) {
            Logger::get_instance().error("CoroutineThreadPoolScheduler: Failed to enqueue task: {}", e.what());
            awaiter->set_exception(std::make_exception_ptr(e));
            schedule_to_main_thread(continuation);
            }
    }



} // namespace Async