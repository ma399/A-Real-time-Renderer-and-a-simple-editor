#pragma once

#include "ThreadPool.h"
#include <memory>
#include <type_traits>

namespace Async {

    class WorkerHookBase {
    public:
        virtual ~WorkerHookBase() = default;
        // Returns true if hook processed something, false if no work was done
        virtual bool execute_hook(size_t worker_index) = 0;
    };

    template<typename T>
    class MemberFunctionHook : public WorkerHookBase {
    public:
        using HookFunction = bool (T::*)(size_t);
        
        MemberFunctionHook(std::shared_ptr<T> instance, HookFunction func) 
            : instance_(std::move(instance)), hook_func_(func) {}
        
        bool execute_hook(size_t worker_index) override {
            if (auto locked = instance_.lock()) {
                return (locked.get()->*hook_func_)(worker_index);
            }
            return false;  // Instance was destroyed
        }
        
    private:
        std::weak_ptr<T> instance_;
        HookFunction hook_func_;
    };

    // Generic callable-based hook 
    template<typename Callable>
    class CallableHook : public WorkerHookBase {
    public:
        static_assert(std::is_invocable_r_v<bool, Callable, size_t>, 
                     "Callable must be invocable with size_t and return bool");
        
        explicit CallableHook(Callable&& callable) 
            : callable_(std::forward<Callable>(callable)) {}
        
        bool execute_hook(size_t worker_index) override {
            return callable_(worker_index);
        }
        
    private:
        std::decay_t<Callable> callable_;
    };


    class EnhancedThreadPool : public ThreadPool {
    public:
        explicit EnhancedThreadPool(size_t numThreads = std::thread::hardware_concurrency());
        ~EnhancedThreadPool();

        // Template method to register a member function hook with shared_ptr 
        template<typename T>
        void register_worker_hook(std::shared_ptr<T> instance, bool (T::*hook_func)(size_t)) {
            worker_hook_ = std::make_unique<MemberFunctionHook<T>>(std::move(instance), hook_func);
        }

        // Template method to register any callable 
        template<typename Callable>
        void register_worker_hook(Callable&& callable) {
            worker_hook_ = std::make_unique<CallableHook<Callable>>(std::forward<Callable>(callable));
        }

        // Remove the hook
        void unregister_worker_hook() {
            worker_hook_.reset();
        }

        static size_t get_current_worker_index();

    protected:
        // override worker thread function, support external hooks
        void workerThread() override;

    private:
        std::unique_ptr<WorkerHookBase> worker_hook_;
        
        // thread local storage
        static thread_local size_t current_worker_index_;
        static thread_local bool worker_index_initialized_;
        
        // worker thread setup
        void setup_worker_thread(size_t worker_index);
    };

} // namespace Async
