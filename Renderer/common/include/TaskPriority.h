#pragma once

#include <cstdint>
#include <functional>
#include <chrono>

namespace Async {

    enum class TaskPriority : uint8_t {
        k_background = 0,  
        k_normal = 1,      
        k_high = 2,        
        k_critical = 3     
    };

    //Get the string representation of a priority level
    constexpr const char* priority_to_string(TaskPriority priority) {
        switch (priority) {
            case TaskPriority::k_background: return "BACKGROUND";
            case TaskPriority::k_normal: return "NORMAL";
            case TaskPriority::k_high: return "HIGH";
            case TaskPriority::k_critical: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    //Task wrapper that includes priority and metadata
    struct PriorityTask {
        std::function<void()> task;
        TaskPriority priority;
        std::chrono::steady_clock::time_point submit_time;
        uint64_t task_id;
        
        PriorityTask(std::function<void()> t, TaskPriority p, uint64_t id = 0)
            : task(std::move(t))
            , priority(p)
            , submit_time(std::chrono::steady_clock::now())
            , task_id(id) {}

        // Move constructor
        PriorityTask(PriorityTask&& other) noexcept
            : task(std::move(other.task))
            , priority(other.priority)
            , submit_time(other.submit_time)
            , task_id(other.task_id) {}

        // Move assignment
        PriorityTask& operator=(PriorityTask&& other) noexcept {
            if (this != &other) {
                task = std::move(other.task);
                priority = other.priority;
                submit_time = other.submit_time;
                task_id = other.task_id;
            }
            return *this;
        }

        // Disable copy operations
        PriorityTask(const PriorityTask&) = delete;
        PriorityTask& operator=(const PriorityTask&) = delete;

        // Comparison for priority queue 
        bool operator<(const PriorityTask& other) const {
            if (priority != other.priority) {
                return priority < other.priority; 
            }
            return submit_time > other.submit_time; // Earlier submission time first (FIFO for same priority)
        }
    };

} // namespace Async

