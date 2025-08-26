#include "CoroutineThreadPoolScheduler.h"
#include "EnhancedThreadPool.h"
#include <fstream>
#include <random>
#include <mutex>

namespace Async {

// Thread local storage definition
thread_local size_t CoroutineThreadPoolScheduler::current_worker_index_ = 0;

CoroutineThreadPoolScheduler::CoroutineThreadPoolScheduler(size_t thread_count)
    : main_thread_id_(std::this_thread::get_id()) {
    
    Logger::get_instance().info("Creating CoroutineThreadPoolScheduler with {} threads", thread_count);
    
    try {
        Logger::get_instance().debug("About to create EnhancedThreadPool...");
        thread_pool_ = std::make_unique<EnhancedThreadPool>(thread_count);
        Logger::get_instance().debug("EnhancedThreadPool created successfully");
        
        if (!thread_pool_) {
            throw std::runtime_error("EnhancedThreadPool creation returned null");
        }
        
        // Initialize work stealing queues - one queue per thread
        Logger::get_instance().debug("Initializing work stealing queues...");
        worker_queues_.reserve(thread_count);
        for (size_t i = 0; i < thread_count; ++i) {
            worker_queues_.emplace_back(std::make_unique<WorkStealingQueue>());
        }
        Logger::get_instance().debug("Work stealing queues initialized");
        
        Logger::get_instance().info("CoroutineThreadPoolScheduler created successfully");
    } catch (const std::exception& e) {
        Logger::get_instance().error("Failed to create CoroutineThreadPoolScheduler: {}", e.what());
        // Ensure thread_pool_ is null on failure
        thread_pool_.reset();
        throw;
    }
}

void CoroutineThreadPoolScheduler::initialize() {
    try {
        // Skip worker hook registration for now to avoid shared_from_this issues
        // The hook will be registered when needed
        Logger::get_instance().info("CoroutineThreadPoolScheduler initialized (hook registration deferred)");
    } catch (const std::exception& e) {
        Logger::get_instance().error("Failed to initialize CoroutineThreadPoolScheduler: {}", e.what());
        throw;
    }
}

CoroutineThreadPoolScheduler::~CoroutineThreadPoolScheduler() {
    if (running_) {
        shutdown(false);
    }
}

void CoroutineThreadPoolScheduler::shutdown(bool wait_for_completion) {
    if (!running_.load()) {
        return;
    }
    
    LOG_INFO("Shutting down CoroutineThreadPoolScheduler (wait_for_completion: {})", wait_for_completion);
    
    running_.store(false);
    
    coroutine_available_.notify_all();
    
    if (thread_pool_) {
        // Unregister worker hook before stopping thread pool
        thread_pool_->unregister_worker_hook();
        thread_pool_->stop(wait_for_completion);
    }

    worker_queues_.clear();
    
    LOG_INFO("CoroutineThreadPoolScheduler shutdown complete");
}

bool CoroutineThreadPoolScheduler::is_running() const noexcept {
    return running_.load() && thread_pool_ && thread_pool_->isRunning();
}

void CoroutineThreadPoolScheduler::schedule_coroutine(std::coroutine_handle<> handle, Async::TaskPriority priority) {
    if (!running_.load()) {
        LOG_WARN("Cannot schedule coroutine - scheduler is not running");
        return;
    }

    uint64_t task_id = next_coroutine_id_.fetch_add(1);
    PriorityCoroutine task(handle, priority, task_id);
    
    LOG_DEBUG("Scheduling coroutine task {} with priority {} ({})",
              task_id, static_cast<int>(priority), Async::priority_to_string(priority));

    distribute_coroutine_to_worker(std::move(task));
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.coroutines_submitted++;
        switch (priority) {
            case Async::TaskPriority::k_background: stats_.priority_stats.background_tasks++; break;
            case Async::TaskPriority::k_normal: stats_.priority_stats.normal_tasks++; break;
            case Async::TaskPriority::k_high: stats_.priority_stats.high_tasks++; break;
            case Async::TaskPriority::k_critical: stats_.priority_stats.critical_tasks++; break;
        }
    }
    
    coroutine_available_.notify_one();
}

bool CoroutineThreadPoolScheduler::try_process_coroutine_in_threadpool(size_t worker_index) {
    if (!running_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::optional<PriorityCoroutine> task_opt;
    
    // 1. First try to get task from local queue
    if (worker_index < worker_queues_.size()) {
        task_opt = worker_queues_[worker_index]->pop();
        if (task_opt.has_value()) {
            LOG_DEBUG("ThreadPool worker {} got coroutine from local queue", worker_index);
        }
    }
    
    // 2. If local queue is empty, try global queue
    if (!task_opt.has_value()) {
        std::unique_lock<std::mutex> lock(global_coroutine_mutex_);
        if (!global_coroutine_queue_.empty()) {
            task_opt = std::move(const_cast<PriorityCoroutine&>(global_coroutine_queue_.top()));
            global_coroutine_queue_.pop();
            LOG_DEBUG("ThreadPool worker {} got coroutine from global queue", worker_index);
        }
    }
    
    // 3. If still no task, try work stealing
    if (!task_opt.has_value()) {
        task_opt = try_steal_coroutine_work(worker_index);
        if (task_opt.has_value()) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.work_steals_successful++;
            LOG_DEBUG("ThreadPool worker {} successfully stole coroutine work", worker_index);
        }
    }
    
    // Execute the coroutine task if found
    if (task_opt.has_value() && task_opt->handle) {
        try {
            LOG_DEBUG("ThreadPool worker {} executing coroutine task {}", worker_index, task_opt->task_id);
            task_opt->handle.resume();
            
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.coroutines_completed++;
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in coroutine execution on worker {}: {}", worker_index, e.what());
        } catch (...) {
            LOG_ERROR("Unknown exception in coroutine execution on worker {}", worker_index);
        }
    }
    
    return false;
}

std::optional<PriorityCoroutine> CoroutineThreadPoolScheduler::try_steal_coroutine_work(size_t current_worker) {
    size_t num_workers = worker_queues_.size();
    if (num_workers <= 1) {
        return std::nullopt;
    }

    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, num_workers - 1);
    
    for (size_t attempts = 0; attempts < num_workers - 1; ++attempts) {
        size_t target_worker = dist(gen);
        if (target_worker == current_worker) {
            target_worker = (target_worker + 1) % num_workers;
        }
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.work_steals_attempted++;
        }
        
        auto stolen_task = worker_queues_[target_worker]->steal();
        if (stolen_task.has_value()) {
            Logger::get_instance().debug("Worker {} stole task from worker {}", current_worker, target_worker);
            return stolen_task;
        }
    }
    
    return std::nullopt;
}

void CoroutineThreadPoolScheduler::distribute_coroutine_to_worker(PriorityCoroutine task) {
    size_t target_worker = get_least_loaded_coroutine_worker();
    worker_queues_[target_worker]->push(std::move(task));
    Logger::get_instance().debug("Distributed coroutine task to worker {}", target_worker);
}

size_t CoroutineThreadPoolScheduler::get_least_loaded_coroutine_worker() const noexcept {
    size_t min_load = SIZE_MAX;
    size_t best_worker = 0;
    
    for (size_t i = 0; i < worker_queues_.size(); ++i) {
        size_t load = worker_queues_[i]->size();
        if (load < min_load) {
            min_load = load;
            best_worker = i;
        }
    }
    
    return best_worker;
}

CoroutineThreadPoolScheduler::Stats CoroutineThreadPoolScheduler::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    Stats result;
    result.coroutines_submitted = stats_.coroutines_submitted.load();
    result.coroutines_completed = stats_.coroutines_completed.load();
    result.work_steals_attempted = stats_.work_steals_attempted.load();
    result.work_steals_successful = stats_.work_steals_successful.load();
    result.priority_stats.background_tasks = stats_.priority_stats.background_tasks.load();
    result.priority_stats.normal_tasks = stats_.priority_stats.normal_tasks.load();
    result.priority_stats.high_tasks = stats_.priority_stats.high_tasks.load();
    result.priority_stats.critical_tasks = stats_.priority_stats.critical_tasks.load();
    return result;
}

void CoroutineThreadPoolScheduler::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.coroutines_submitted = 0;
    stats_.coroutines_completed = 0;
    stats_.work_steals_attempted = 0;
    stats_.work_steals_successful = 0;
    stats_.priority_stats.background_tasks = 0;
    stats_.priority_stats.normal_tasks = 0;
    stats_.priority_stats.high_tasks = 0;
    stats_.priority_stats.critical_tasks = 0;
}

// Context switching implementation
Task<void> CoroutineThreadPoolScheduler::SwitchToMain() {
    Logger::get_instance().debug("SwitchToMain requested");
    
    ContextSwitchAwaiter awaiter(this, true);
    co_await awaiter;
    
    Logger::get_instance().debug("SwitchToMain completed");
    co_return;
}

Task<void> CoroutineThreadPoolScheduler::SwitchToThreadPool() {
    Logger::get_instance().debug("SwitchToThreadPool requested");
    
    ContextSwitchAwaiter awaiter(this, false);
    co_await awaiter;
    
    Logger::get_instance().debug("SwitchToThreadPool completed");
    co_return;
}

size_t CoroutineThreadPoolScheduler::process_main_thread_coroutines() {
    if (std::this_thread::get_id() != main_thread_id_) {
        LOG_WARN("process_main_thread_coroutines called from non-main thread");
        return 0;
    }

    size_t processed_count = 0;
    std::queue<std::coroutine_handle<>> handles_to_process;
    
    // Gather all pending handles
    {
        std::lock_guard<std::mutex> lock(main_thread_mutex_);
        handles_to_process.swap(main_thread_queue_);
    }
    
    // Process all handles
    while (!handles_to_process.empty()) {
        auto handle = handles_to_process.front();
        handles_to_process.pop();
        
        if (handle) {
            processed_count++;
            try {
                if (!handle) {
                    LOG_WARN("Null coroutine handle encountered");
                    continue;
                }
                
                if (handle.done()) {
                    LOG_DEBUG("Skipping already completed coroutine");
                    continue;
                }
                
                LOG_DEBUG("Processing main thread coroutine");
                handle.resume();
            } catch (const std::exception& e) {
                LOG_ERROR("Exception in main thread coroutine resume: {}", e.what());
            } catch (...) {
                LOG_ERROR("Unknown exception in main thread coroutine resume");
            }
        }
    }
    
    if (processed_count > 0) {
        LOG_DEBUG("Processed {} main thread coroutines", processed_count);
    }
    
    return processed_count;
}

// Global instance
CoroutineThreadPoolScheduler& CoroutineThreadPoolScheduler::get_instance() {
    static std::mutex instance_mutex;
    static std::shared_ptr<CoroutineThreadPoolScheduler> instance;
    static bool initialization_failed = false;
    
    std::lock_guard<std::mutex> lock(instance_mutex);
    
    if (initialization_failed) {
        throw std::runtime_error("CoroutineThreadPoolScheduler initialization previously failed");
    }
    
    if (!instance) {
        try {
            Logger::get_instance().debug("Creating CoroutineThreadPoolScheduler singleton instance...");
            instance = std::shared_ptr<CoroutineThreadPoolScheduler>(new CoroutineThreadPoolScheduler());
            Logger::get_instance().debug("CoroutineThreadPoolScheduler instance created, initializing...");
            
            // Verify the instance was created properly
            if (!instance->thread_pool_) {
                throw std::runtime_error("CoroutineThreadPoolScheduler thread_pool_ is null after construction");
            }
            
            // Delay initialization until after shared_ptr is fully constructed
            instance->initialize();
            Logger::get_instance().info("CoroutineThreadPoolScheduler singleton initialized successfully");
        } catch (const std::exception& e) {
            Logger::get_instance().error("Failed to create/initialize CoroutineThreadPoolScheduler in get_instance: {}", e.what());
            instance.reset(); // Clear the instance on failure
            initialization_failed = true;
            throw;
        }
    }
    
    if (!instance) {
        throw std::runtime_error("CoroutineThreadPoolScheduler instance is null after initialization");
    }
    
    return *instance;
}

void CoroutineThreadPoolScheduler::schedule_to_main_thread(std::coroutine_handle<> handle) {
    std::lock_guard<std::mutex> lock(main_thread_mutex_);
    main_thread_queue_.push(handle);
}

void CoroutineThreadPoolScheduler::schedule_to_thread_pool(std::coroutine_handle<> handle) {
    schedule_coroutine(handle, Async::TaskPriority::k_normal);
}

// ContextSwitchAwaiter implementation
void ContextSwitchAwaiter::await_suspend(std::coroutine_handle<> handle) {
    if (switch_to_main_) {
        scheduler_->schedule_to_main_thread(handle);
    } else {
        scheduler_->schedule_to_thread_pool(handle);
    }
}

} // namespace Async