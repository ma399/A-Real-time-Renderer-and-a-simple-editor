#pragma once

#include "TaskPriority.h"
#include <queue>
#include <mutex>
#include <array>
#include <atomic>

namespace Async {

    // TODO: Write a lock-free version using atomic operations and memory orderings
    class PriorityTaskQueue {
    private:
        // Separate queue for each priority level
        std::array<std::queue<PriorityTask>, 4> priority_queues_;
        
        // Mutex for thread safety
        mutable std::mutex mutex_;
        
        // Statistics
        std::atomic<uint64_t> total_tasks_submitted_{0};
        std::atomic<uint64_t> total_tasks_processed_{0};
        std::array<std::atomic<uint64_t>, 4> priority_task_counts_{};
        
        // Task ID generator
        std::atomic<uint64_t> next_task_id_{1};

        // Helper function to get priority index
        static constexpr size_t priority_index(TaskPriority priority) {
            return static_cast<size_t>(priority);
        }

    public:
        PriorityTaskQueue() = default;
        
        // Disable copy operations
        PriorityTaskQueue(const PriorityTaskQueue&) = delete;
        PriorityTaskQueue& operator=(const PriorityTaskQueue&) = delete;
        
        // Enable move operations
        PriorityTaskQueue(PriorityTaskQueue&&) = default;
        PriorityTaskQueue& operator=(PriorityTaskQueue&&) = default;

        uint64_t submit(std::function<void()> task, TaskPriority priority = TaskPriority::k_normal) {
            uint64_t task_id = next_task_id_.fetch_add(1);
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                size_t idx = priority_index(priority);
                priority_queues_[idx].emplace(std::move(task), priority, task_id);
                priority_task_counts_[idx].fetch_add(1);
            }
            
            total_tasks_submitted_.fetch_add(1);
            return task_id;
        }


        bool try_pop(PriorityTask& task) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Check queues from highest to lowest priority
            for (int i = 3; i >= 0; --i) {
                if (!priority_queues_[i].empty()) {
                    task = std::move(priority_queues_[i].front());
                    priority_queues_[i].pop();
                    
                    // Update statistics
                    priority_task_counts_[i].fetch_sub(1);
                    total_tasks_processed_.fetch_add(1);
                    return true;
                }
            }
            
            return false;
        }


        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& queue : priority_queues_) {
                if (!queue.empty()) {
                    return false;
                }
            }
            return true;
        }


        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t total = 0;
            for (const auto& queue : priority_queues_) {
                total += queue.size();
            }
            return total;
        }


        size_t size(TaskPriority priority) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return priority_queues_[priority_index(priority)].size();
        }


        struct Statistics {
            uint64_t total_submitted;
            uint64_t total_processed;
            uint64_t total_pending;
            std::array<uint64_t, 4> priority_pending;
            std::array<uint64_t, 4> priority_submitted;
        };

        Statistics get_statistics() const {
            std::lock_guard<std::mutex> lock(mutex_);
            
            Statistics stats;
            stats.total_submitted = total_tasks_submitted_.load();
            stats.total_processed = total_tasks_processed_.load();
            stats.total_pending = stats.total_submitted - stats.total_processed;
            
            for (size_t i = 0; i < 4; ++i) {
                stats.priority_pending[i] = priority_queues_[i].size();
                stats.priority_submitted[i] = priority_task_counts_[i].load() + stats.priority_pending[i];
            }
            
            return stats;
        }


        void clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& queue : priority_queues_) {
                while (!queue.empty()) {
                    queue.pop();
                }
            }
        }
    };

} // namespace Async
