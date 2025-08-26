#include "ThreadPool.h"

namespace Async {

    ThreadPool::ThreadPool(size_t numThreads)
        : stop_(false)
        , activeThreads_(0)
        , completedTasks_(0)
        , numThreads_(numThreads) {
        
        if (numThreads == 0) {
            throw std::invalid_argument("ThreadPool: Number of threads must be greater than 0");
        }

        LOG_INFO("ThreadPool: Initializing thread pool with {} threads", numThreads);

        try {
            workers_.reserve(numThreads);
            
            for (size_t i = 0; i < numThreads; ++i) {
                workers_.emplace_back(&ThreadPool::workerThread, this);
                LOG_DEBUG("ThreadPool: Created worker thread {}", i);
            }
            
            LOG_INFO("ThreadPool: Successfully initialized with {} worker threads", workers_.size());
        } catch (const std::exception& e) {
            LOG_ERROR("ThreadPool: Failed to initialize: {}", e.what());
            
            // Clean up any threads that were created before the exception
            stop_.store(true);
            condition_.notify_all();
            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            throw;
        }
    }

    ThreadPool::~ThreadPool() {
        if (!stop_.load()) {
            LOG_INFO("ThreadPool: Shutting down thread pool from destructor");
            stop(true);
        }
    }

    void ThreadPool::workerThread() {
        LOG_DEBUG("ThreadPool: Worker thread started");
        
        while (true) {
            Async::PriorityTask priority_task([]{}, Async::TaskPriority::k_normal);
            bool has_task = false;
            
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                
                // Wait for a task or stop signal
                condition_.wait(lock, [this] {
                    return stop_.load() || !priority_queue_.empty();
                });
                
                // Exit if stopped and no more tasks
                if (stop_.load() && priority_queue_.empty()) {
                    break;
                }
                
                // Get the highest priority task
                has_task = priority_queue_.try_pop(priority_task);
                if (has_task) {
                    LOG_DEBUG("ThreadPool: Worker picked up task with priority {} ({}), remaining: {}", 
                            static_cast<int>(priority_task.priority), 
                            Async::priority_to_string(priority_task.priority),
                            priority_queue_.size());
                }
            }
            
            // Execute the task
            if (has_task && priority_task.task) {
                try {
                    activeThreads_.fetch_add(1);
                    LOG_DEBUG("ThreadPool: Executing task ID {} with priority {}, active threads: {}", 
                            priority_task.task_id, static_cast<int>(priority_task.priority), activeThreads_.load());
                    
                    auto start_time = std::chrono::steady_clock::now();
                    priority_task.task();
                    auto end_time = std::chrono::steady_clock::now();
                    
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    completedTasks_.fetch_add(1);
                    
                    LOG_DEBUG("ThreadPool: Task {} completed in {}ms, total completed: {}", 
                            priority_task.task_id, duration.count(), completedTasks_.load());
                } catch (const std::exception& e) {
                    LOG_ERROR("ThreadPool: Exception in worker thread (task {}): {}", 
                            priority_task.task_id, e.what());
                }
                
                activeThreads_.fetch_sub(1);
                
                // Notify waiting threads that a task completed
                finishedCondition_.notify_all();
            }
        }
        
        LOG_DEBUG("ThreadPool: Worker thread exiting");
    }

    size_t ThreadPool::get_thread_count() const {
        return numThreads_;
    }

    size_t ThreadPool::get_pending_task_count() const {
        return priority_queue_.size();
    }

    size_t ThreadPool::getActiveTaskCount() const {
        return activeThreads_.load();
    }

    bool ThreadPool::isRunning() const {
        return !stop_.load();
    }

    void ThreadPool::stop(bool waitForCompletion) {
        LOG_INFO("ThreadPool: Stopping thread pool (waitForCompletion: {})", waitForCompletion);
        
        if (stop_.load()) {
            LOG_DEBUG("ThreadPool: Thread pool already stopped");
            return;
        }
        
        stop_.store(true);
        
        if (waitForCompletion) {
            LOG_INFO("ThreadPool: Waiting for all tasks to complete...");
            waitForAll();
        } else {
            LOG_WARN("ThreadPool: Stopping immediately, pending tasks will be cancelled");
        }
        
        // Notify all worker threads to wake up and check stop condition
        condition_.notify_all();
        
        // Join all worker threads
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
                LOG_DEBUG("ThreadPool: Worker thread joined");
            }
        }
        
        // Clear any remaining tasks
        {
            size_t remainingTasks = priority_queue_.size();
            priority_queue_.clear();
            if (remainingTasks > 0) {
                LOG_WARN("ThreadPool: Cancelled {} pending tasks", remainingTasks);
            }
        }
        
        LOG_INFO("ThreadPool: Thread pool stopped successfully");
    }

    void ThreadPool::waitForAll() {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        finishedCondition_.wait(lock, [this] {
            return priority_queue_.empty() && activeThreads_.load() == 0;
        });
        
        LOG_DEBUG("ThreadPool: All tasks completed");
    }

    ThreadPool::Statistics ThreadPool::getStatistics() const {
        auto queue_stats = priority_queue_.get_statistics();
        
        Statistics stats;
        stats.totalThreads = numThreads_;
        stats.activeThreads = activeThreads_.load();
        stats.pendingTasks = queue_stats.total_pending;
        stats.completedTasks = completedTasks_.load();
        stats.isRunning = !stop_.load();
        
        // Fill in priority-specific statistics
        stats.priorityStats.backgroundTasks = queue_stats.priority_pending[0];
        stats.priorityStats.normalTasks = queue_stats.priority_pending[1];
        stats.priorityStats.highTasks = queue_stats.priority_pending[2];
        stats.priorityStats.criticalTasks = queue_stats.priority_pending[3];
        
        return stats;
    }
} // namespace Async
