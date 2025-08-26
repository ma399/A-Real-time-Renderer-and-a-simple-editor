#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <mutex>
#include <condition_variable>

#include <Logger.h>

namespace Async {

    // Forward declarations
    template<typename T>
    class Task;
    
    template<typename T>
    class TaskPromise;

    // Base class for promise functionality
    template<typename T>
    class TaskPromiseBase {
    public:
        using value_type = T;
        using handle_type = std::coroutine_handle<TaskPromise<T>>;

        TaskPromiseBase() = default;
        ~TaskPromiseBase() = default;

        // Common coroutine interface
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept { 
            struct final_awaiter {
                bool await_ready() noexcept { return false; }
                
                std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise<T>> handle) noexcept {
                    auto& promise = handle.promise();
                    
                    // Notify any threads waiting in sync_wait
                    promise.notify_completion();
                    
                    if (promise.awaiting_coroutine_) {
                        return promise.awaiting_coroutine_;
                    } else {
                        return std::noop_coroutine();
                    }
                }
                
                void await_resume() noexcept {}
            };
            return final_awaiter{};
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        bool has_exception() const noexcept {
            return exception_ != nullptr;
        }

        // Notify any waiting sync_wait calls
        void notify_completion() noexcept {
            std::lock_guard<std::mutex> lock(completion_mutex_);
            completion_cv_.notify_all();
        }

        // Wait for completion
        void wait_for_completion(std::coroutine_handle<TaskPromise<T>> handle) const {
            std::unique_lock<std::mutex> lock(completion_mutex_);
            completion_cv_.wait(lock, [handle]() { 
                return handle.done(); 
            });
        }

        std::coroutine_handle<> awaiting_coroutine_{};

    protected:
        std::exception_ptr exception_{};
        mutable std::mutex completion_mutex_{};
        mutable std::condition_variable completion_cv_{};
    };

    // Specialization for non-void types
    template<typename T>
    class TaskPromise : public TaskPromiseBase<T> {
        friend class Task<T>;
    public:
        using base = TaskPromiseBase<T>;
        using typename base::value_type;
        using typename base::handle_type;

        TaskPromise() = default;
        ~TaskPromise() = default;

        // Coroutine interface
        Task<T> get_return_object();

        // For non-void types only
        template<typename V>
        void return_value(V&& value) {
            result_.emplace(std::forward<V>(value));   
        }

        T get_result() {
            if (base::exception_) {
                std::rethrow_exception(base::exception_);
            }
            if (result_.has_value()) {
                return std::move(result_.value());
            }
            throw std::runtime_error("Task: No result available");
        }

        bool has_result() const noexcept {
            return result_.has_value() && !base::exception_;
        }

    private:
        std::optional<T> result_{};
    };

    // Specialization for void type
    template<>
    class TaskPromise<void> : public TaskPromiseBase<void> {
        friend class Task<void>;
    public:
        using base = TaskPromiseBase<void>;
        using typename base::value_type;
        using typename base::handle_type;

        TaskPromise() = default;
        ~TaskPromise() = default;

        // Coroutine interface
        Task<void> get_return_object();

        // For void type only
        void return_void() noexcept {
        }

        void get_result() {
            if (base::exception_) {
                std::rethrow_exception(base::exception_);
            }
        }

        bool has_result() const noexcept {
            return !base::exception_;
        }
    };

   

    // The Task class
    template<typename T = void>
    class Task {
    public:
        using promise_type = TaskPromise<T>;
        using handle_type = std::coroutine_handle<promise_type>;

        explicit Task(handle_type handle) : handle_(handle) {
            // Auto-start the coroutine when Task is created
            if (handle_ && !handle_.done()) {
                handle_.resume();
            }
        }

        // Disable copy, enable move
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {
        }
        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                if (handle_) {
                    handle_.destroy();
                }
                handle_ = std::exchange(other.handle_, {});
            }
            return *this;
        }

        ~Task() {
            if (handle_) {
                handle_.destroy();
            }
        }

        // Check if task is ready (non-blocking)
        bool is_ready() const noexcept {
            return handle_ && handle_.done();
        }

        // Check if task has completed successfully (non-blocking)
        bool is_completed() const noexcept {
            return is_ready() && !handle_.promise().has_exception();
        }

        // Check if task has failed with an exception (non-blocking)
        bool has_exception() const noexcept {
            return is_ready() && handle_.promise().has_exception();
        }

        // Check if task is valid
        bool valid() const noexcept {
            return static_cast<bool>(handle_);
        }

        // Resume the task
        void resume() {
            if (handle_ && !handle_.done()) {
                handle_.resume();
            }
        }

        // Blocking wait for result
        auto sync_wait() {
            if (!handle_) {
                throw std::runtime_error("Task: Invalid task handle");
            }
            
            // Start the coroutine if not already running
            if (!handle_.done()) {
                handle_.resume();
            }
            
            // Wait efficiently using promise's method
            handle_.promise().wait_for_completion(handle_);
            
            if constexpr (std::is_void_v<T>) {
                handle_.promise().get_result();
            } else {
                return handle_.promise().get_result();
            }
        }

        // Get the result if ready (non-blocking) - for non-void types
        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U>, std::optional<U>> try_get() {
            if (!is_ready()) {
                return std::nullopt;
            }
            
            if (handle_.promise().has_exception()) {
                handle_.promise().get_result(); 
            }
            
            return handle_.promise().get_result();
        }

        // For void type, returns true if completed successfully, throws if exception
        template<typename U = T>
        std::enable_if_t<std::is_void_v<U>, bool> try_get() {
            if (!is_ready()) {
                return false;
            }
            
            handle_.promise().get_result(); 
            return true;
        }

        // Legacy name for backward compatibility
        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U>, std::optional<U>> try_get_result() {
            return try_get<U>();
        }

        template<typename U = T>
        std::enable_if_t<std::is_void_v<U>, bool> try_get_result() {
            return try_get<U>();
        }

        // Coroutine awaiter interface
        bool await_ready() const noexcept {
            return handle_ && handle_.done();
        }

        template<typename Promise>
        void await_suspend(std::coroutine_handle<Promise> awaiting_coroutine) noexcept {
            LOG_DEBUG("Task await_suspend: Storing awaiting coroutine");
            
            // Store the awaiting coroutine in the promise for later resumption
            if (handle_) {
                handle_.promise().awaiting_coroutine_ = awaiting_coroutine;
                LOG_DEBUG("Task await_suspend: Stored in promise");
            } else {
                LOG_ERROR("Task await_suspend: Invalid handle");
            }
            
        }

        auto await_resume() {
            if constexpr (std::is_void_v<T>) {
                sync_wait();
            } else {
                return sync_wait();
            }
        }

        // Get the underlying handle
        handle_type handle() const noexcept {
            return handle_;
        }

    private:
        handle_type handle_;
    };

    // Promise method implementations
    template<typename T>
    Task<T> TaskPromise<T>::get_return_object() {
        return Task<T>{handle_type::from_promise(*this)};
    }

    inline Task<void> TaskPromise<void>::get_return_object() {
        return Task<void>{handle_type::from_promise(*this)};
    }

} // namespace Async