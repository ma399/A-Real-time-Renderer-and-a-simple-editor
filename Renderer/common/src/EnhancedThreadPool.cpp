#include "EnhancedThreadPool.h"

namespace Async {

    thread_local size_t EnhancedThreadPool::current_worker_index_ = 0;
    thread_local bool EnhancedThreadPool::worker_index_initialized_ = false;

    EnhancedThreadPool::EnhancedThreadPool(size_t numThreads) : ThreadPool(numThreads) {
        LOG_INFO("EnhancedThreadPool: Initialized with {} threads (hook-enabled)", numThreads);
    }

    EnhancedThreadPool::~EnhancedThreadPool() = default;

    size_t EnhancedThreadPool::get_current_worker_index() {
        return current_worker_index_;
    }

    void EnhancedThreadPool::setup_worker_thread(size_t worker_index) {
        if (!worker_index_initialized_) {
            current_worker_index_ = worker_index;
            worker_index_initialized_ = true;
            LOG_DEBUG("EnhancedThreadPool: Worker {} initialized", worker_index);
        }
    }

    void EnhancedThreadPool::workerThread() {
        
        static std::atomic<size_t> next_worker_index{0};
        size_t worker_index = next_worker_index.fetch_add(1, std::memory_order_relaxed);
        setup_worker_thread(worker_index);
        
        LOG_DEBUG("EnhancedThreadPool: Worker thread {} started", worker_index);
        
        while (true) {
            Async::PriorityTask priority_task([]{}, TaskPriority::k_normal);
            bool has_task = false;
            
            // try to execute external worker hook (e.g., coroutine processing)
            bool processed_external = false;
            if (worker_hook_) {
                try {
                    processed_external = worker_hook_->execute_hook(worker_index);
                        } catch (const std::exception& e) {
            LOG_ERROR("EnhancedThreadPool: Exception in worker hook for thread {}: {}",
                                      worker_index, e.what());
        } catch (...) {
            LOG_ERROR("EnhancedThreadPool: Unknown exception in worker hook for thread {}",
                                      worker_index);
        }
            }
            
            // process regular thread pool tasks
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                
                // wait for task or stop signal (only if no external work was processed)
                if (!processed_external) {
                    condition_.wait(lock, [this] {
                        return stop_.load() || !priority_queue_.empty();
                    });
                }
                
                // exit condition check
                if (stop_.load() && priority_queue_.empty()) {
                    break;
                }
                
                // get highest priority task
                has_task = priority_queue_.try_pop(priority_task);
                if (has_task) {
                    LOG_DEBUG("EnhancedThreadPool: Worker {} picked up task with priority {} ({}), remaining: {}", 
                             worker_index, static_cast<int>(priority_task.priority), 
                             priority_to_string(priority_task.priority),
                             priority_queue_.size());
                }
            }
            
            // execute task
            if (has_task && priority_task.task) {
                try {
                    activeThreads_.fetch_add(1, std::memory_order_relaxed);
                    LOG_DEBUG("EnhancedThreadPool: Worker {} executing task ID {} with priority {}, active threads: {}", 
                             worker_index, priority_task.task_id, static_cast<int>(priority_task.priority), activeThreads_.load());
                    
                    auto start_time = std::chrono::steady_clock::now();
                    priority_task.task();
                    auto end_time = std::chrono::steady_clock::now();
                    
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    completedTasks_.fetch_add(1, std::memory_order_relaxed);
                    
                    LOG_DEBUG("EnhancedThreadPool: Worker {} completed task {} in {}ms, total completed: {}", 
                             worker_index, priority_task.task_id, duration.count(), completedTasks_.load());
                } catch (const std::exception& e) {
                    LOG_ERROR("EnhancedThreadPool: Exception in worker thread {} (task {}): {}", 
                             worker_index, priority_task.task_id, e.what());
                }
                
                activeThreads_.fetch_sub(1, std::memory_order_relaxed);
                
                // notify waiting threads that the task is completed
                finishedCondition_.notify_all();
            }
        }
        
        LOG_DEBUG("EnhancedThreadPool: Worker thread {} exiting", worker_index);
    }

} // namespace Async
