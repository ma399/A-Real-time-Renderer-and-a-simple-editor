#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include "Model.h"
#include "Texture.h"
#include "Material.h"
#include "Task.h"
#include "TaskPriority.h"
#include "CoroutineThreadPoolScheduler.h"
#include "AssimpLoader.h"
#include "Logger.h"

// Forward declarations
class Shader;
class Scene;
class Light;


class CoroutineResourceManager {
public:
    explicit CoroutineResourceManager();
    ~CoroutineResourceManager();

    CoroutineResourceManager(const CoroutineResourceManager&) = delete;
    CoroutineResourceManager& operator=(const CoroutineResourceManager&) = delete;
    CoroutineResourceManager(CoroutineResourceManager&&) = delete;
    CoroutineResourceManager& operator=(CoroutineResourceManager&&) = delete;

    std::unique_ptr<Scene> create_simple_scene();

    // Load Resource
    template<typename T>
    std::shared_ptr<T> load(const std::string& path);
    template<typename T>
    Async::Task<std::shared_ptr<T>> load_async(const std::string& path,
                                             std::function<void(float, const std::string&)> progress_callback,
                                             Async::TaskPriority priority = Async::TaskPriority::k_normal);
    template<typename T>
    Async::Task<void> preload_async(const std::vector<std::string>& paths,
                                  Async::TaskPriority priority = Async::TaskPriority::k_background);

    Async::Task<std::shared_ptr<Model>> assemble_model_async(const std::string& model_name,
                                                          const std::vector<std::string>& mesh_paths,
                                                          const std::vector<std::string>& material_paths = {},
                                                          Async::TaskPriority priority = Async::TaskPriority::k_normal);

    // Model assembling 
    std::shared_ptr<Model> assemble_model(const std::string& mesh_path, const std::string& material_path);
    std::shared_ptr<Model> assumble_model(const Mesh& mesh, const Material& material);


    // Get Textures from material
    std::shared_ptr<Texture> get_material_texture(const Material& material, const std::string& texture_name);
    std::unordered_map<std::string, std::shared_ptr<Texture>> get_material_textures(const Material& material);

    void set_material_texture(Material& material, const std::string& texture_name, const std::string& texture_path);
    std::vector<std::shared_ptr<Model>> get_scene_models(const class Scene& scene);

    std::vector<std::shared_ptr<class Light>> get_scene_lights(const class Scene& scene) const;

    void store_light_in_cache(const std::string& light_id, std::shared_ptr<class Light> light);

    std::shared_ptr<class Shader> create_shader(const std::string& shader_name,
                                              const std::string& vertex_path,
                                              const std::string& fragment_path,
                                              const std::string& geometry_path = "",
                                              const std::string& compute_path = "");

    // Get shader
    std::shared_ptr<class Shader> get_shader(const std::string& shader_name) const;

    void store_shader(const std::string& shader_name, std::shared_ptr<class Shader> shader);
    void remove_shader(const std::string& shader_name);
    std::vector<std::string> get_shader_names() const;

    // Irradiance map generation and management
    std::shared_ptr<Texture> compute_irradiance_map(const std::string& skybox_texture_name, int irradiance_size = 32);
    void store_irradiance_map(const std::string& skybox_texture_name, std::shared_ptr<Texture> irradiance_map);
    std::shared_ptr<Texture> get_irradiance_map(const std::string& skybox_texture_name) const;

    template<typename T>
    bool is_loaded(const std::string& path) const;

    template<typename T>
    std::shared_ptr<T> get(const std::string& path) const;

    template<typename T>
    void unload(const std::string& path);

    template<typename T>
    void clear_cache();

    void clear_all_caches();

    size_t get_cache_size() const;

    template<typename T>
    std::vector<std::string> get_cached_resource_names() const;
    
    struct StatsObserver {
        size_t total_loads = 0;
        size_t task_cache_hits = 0;
        size_t task_cache_misses = 0;
        size_t async_loads_requested = 0;
        size_t async_loads_completed = 0;
        
        size_t duplicate_requests_avoided = 0; 
        
        size_t priority_loads[4] = {0, 0, 0, 0}; // [background, normal, high, critical]
    };

    struct Stats {
        std::atomic<size_t> total_loads = 0;
        std::atomic<size_t> task_cache_hits = 0;
        std::atomic<size_t> task_cache_misses = 0;
        std::atomic<size_t> async_loads_requested = 0;
        std::atomic<size_t> async_loads_completed = 0;
        
        // Dual-cache specific statistics       
        std::atomic<size_t> duplicate_requests_avoided = 0; 
        
        std::atomic<size_t> priority_loads[4] = {0, 0, 0, 0}; // [background, normal, high, critical]
        
    };

    StatsObserver get_stats() const;
    void reset_stats();

private:
    // Resources cache 
    std::unordered_map<std::string, std::shared_ptr<Mesh>> mesh_cache_;
    std::unordered_map<std::string, std::shared_ptr<Texture>> texture_cache_;
    std::unordered_map<std::string, std::shared_ptr<Material>> material_cache_;
    std::unordered_map<std::string, std::shared_ptr<Model>> model_cache_;
    std::unordered_map<std::string, std::shared_ptr<Light>> light_cache_;
    std::unordered_map<std::string, std::shared_ptr<class Shader>> shader_cache_;
    std::unordered_map<std::string, std::shared_ptr<Texture>> irradiance_cache_;

    // Task cache
    std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Mesh>>>> mesh_task_cache_;
    std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Texture>>>> texture_task_cache_;
    std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Material>>>> material_task_cache_;
    std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Model>>>> model_task_cache_;

    // coroutine scheduler reference
    Async::CoroutineThreadPoolScheduler* scheduler_;
    
    // AssimpLoader instance for direct mesh loading
    std::unique_ptr<AssimpLoader> assimp_loader_;

    // thread safe for both resource cache and task cache
    mutable std::shared_mutex cache_mutex_;     

    // statistics
    mutable Stats stats_;

    // internal coroutine load functions
    Async::Task<std::shared_ptr<Mesh>> load_mesh_async(const std::string& path, Async::TaskPriority priority);
    Async::Task<std::shared_ptr<Texture>> load_texture_async(const std::string& path, Async::TaskPriority priority);
    Async::Task<std::shared_ptr<Material>> load_material_async(const std::string& path, Async::TaskPriority priority);
    Async::Task<std::shared_ptr<Model>> load_model_async(const std::string& path, Async::TaskPriority priority);

    // load functions with progress callback
    Async::Task<std::shared_ptr<Mesh>> load_mesh_async(const std::string& path, 
                                                    std::function<void(float, const std::string&)> progress_callback,
                                                    Async::TaskPriority priority);

    // cache management helper functions
    template<typename T>
    std::unordered_map<std::string, std::shared_ptr<T>>& get_cache();

    template<typename T>
    const std::unordered_map<std::string, std::shared_ptr<T>>& get_cache() const;

    // task cache management helper functions
    template<typename T>
    std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<T>>>>& get_task_cache();

    template<typename T>
    const std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<T>>>>& get_task_cache() const;

    // dual-cache management methods
    template<typename T>
    std::shared_ptr<T> check_resource_cache(const std::string& normalized_path) const;

    template<typename T>
    std::shared_ptr<Async::Task<std::shared_ptr<T>>> check_task_cache(const std::string& normalized_path) const;

    template<typename T>
    void cache_task(const std::string& normalized_path, std::shared_ptr<Async::Task<std::shared_ptr<T>>> task);

    template<typename T>
    void cleanup_task_cache(const std::string& normalized_path);

    // resource validation and path handling
    bool validate_resource_path(const std::string& path) const;
    std::string normalize_resource_path(const std::string& path) const;
    
    // update statistics
    void update_stats(Async::TaskPriority priority, bool cache_hit) const;
};

// Template specialization: get_cache 

template<>
inline std::unordered_map<std::string, std::shared_ptr<Mesh>>& CoroutineResourceManager::get_cache<Mesh>() {
    return mesh_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Mesh>>& CoroutineResourceManager::get_cache<Mesh>() const {
    return mesh_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Texture>>& CoroutineResourceManager::get_cache<Texture>() {
    return texture_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Texture>>& CoroutineResourceManager::get_cache<Texture>() const {
    return texture_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Material>>& CoroutineResourceManager::get_cache<Material>() {
    return material_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Material>>& CoroutineResourceManager::get_cache<Material>() const {
    return material_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Model>>& CoroutineResourceManager::get_cache<Model>() {
    return model_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Model>>& CoroutineResourceManager::get_cache<Model>() const {
    return model_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Light>>& CoroutineResourceManager::get_cache<Light>() {
    return light_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Light>>& CoroutineResourceManager::get_cache<Light>() const {
    return light_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Shader>>& CoroutineResourceManager::get_cache<Shader>() {
    return shader_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Shader>>& CoroutineResourceManager::get_cache<Shader>() const {
    return shader_cache_;
}

// Task cache template specializations 

template<>
inline std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Mesh>>>>& CoroutineResourceManager::get_task_cache<Mesh>() {
    return mesh_task_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Mesh>>>>& CoroutineResourceManager::get_task_cache<Mesh>() const {
    return mesh_task_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Texture>>>>& CoroutineResourceManager::get_task_cache<Texture>() {
    return texture_task_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Texture>>>>& CoroutineResourceManager::get_task_cache<Texture>() const {
    return texture_task_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Material>>>>& CoroutineResourceManager::get_task_cache<Material>() {
    return material_task_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Material>>>>& CoroutineResourceManager::get_task_cache<Material>() const {
    return material_task_cache_;
}

template<>
inline std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Model>>>>& CoroutineResourceManager::get_task_cache<Model>() {
    return model_task_cache_;
}

template<>
inline const std::unordered_map<std::string, std::shared_ptr<Async::Task<std::shared_ptr<Model>>>>& CoroutineResourceManager::get_task_cache<Model>() const {
    return model_task_cache_;
}

// Template function implementation 

template<typename T>
std::shared_ptr<T> CoroutineResourceManager::load(const std::string& path) {
    LOG_DEBUG("CoroutineResourceManager: Synchronous load requested for {}", path);
    
    // check cache first
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        std::string normalized_path = normalize_resource_path(path);
        const auto& cache = get_cache<T>();
        auto it = cache.find(normalized_path);
        if (it != cache.end()) {
            update_stats(Async::TaskPriority::k_normal, true);
            LOG_DEBUG("CoroutineResourceManager: Found in cache: {}", normalized_path);
            return it->second;
        }
    }
    
    auto progress_callback = [](float, const std::string&) {};
    auto task = load_async<T>(path, progress_callback, Async::TaskPriority::k_normal);
    return task.sync_wait();
}

template<typename T>
Async::Task<std::shared_ptr<T>> CoroutineResourceManager::load_async(const std::string& path,
                                                                   std::function<void(float, const std::string&)> progress_callback,
                                                                   Async::TaskPriority priority) {
    // CRITICAL: This should show up in logs if the function is called!
            LOG_INFO("load_async template called: {}", path);
    
    if constexpr (std::is_same_v<T, Mesh>) {
        LOG_INFO("Dispatching to load_mesh_async");
        co_return co_await load_mesh_async(path, progress_callback, priority);
    } else {
        // Other types don't support progress callback yet, return nullptr
        LOG_WARN("Progress callback not supported for this type");
        co_return nullptr;
    }
}

template<typename T>
Async::Task<void> CoroutineResourceManager::preload_async(const std::vector<std::string>& paths, Async::TaskPriority priority) {
            LOG_INFO("CoroutineResourceManager: Preloading {} resources with priority {}", 
                              paths.size(), Async::priority_to_string(priority));
    
    std::vector<Async::Task<std::shared_ptr<T>>> tasks;
    tasks.reserve(paths.size());
    
    for (const auto& path : paths) {
        tasks.emplace_back(load_async<T>(path, priority));
    }
    
    for (auto& task : tasks) {
        co_await task;
    }
    
    LOG_INFO("CoroutineResourceManager: Preloading completed for {} resources", paths.size());
}

template<typename T>
bool CoroutineResourceManager::is_loaded(const std::string& path) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    std::string normalized_path = normalize_resource_path(path);
    const auto& cache = get_cache<T>();
    bool loaded = cache.find(normalized_path) != cache.end();
    
    LOG_DEBUG("CoroutineResourceManager: Cache check for {}: {}", 
                               normalized_path, loaded ? "FOUND" : "NOT FOUND");
    return loaded;
}

template<typename T>
std::shared_ptr<T> CoroutineResourceManager::get(const std::string& path) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    std::string normalized_path = normalize_resource_path(path);
    const auto& cache = get_cache<T>();
    auto it = cache.find(normalized_path);
    
    if (it != cache.end()) {
        //LOG_DEBUG("CoroutineResourceManager: Retrieved cached resource: {}", normalized_path);
        return it->second;
    }
    
    LOG_DEBUG("CoroutineResourceManager: Resource not found in cache: {}", normalized_path);
    return nullptr;
}

template<typename T>
void CoroutineResourceManager::unload(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    std::string normalized_path = normalize_resource_path(path);
    auto& cache = get_cache<T>();
    
    auto it = cache.find(normalized_path);
    if (it != cache.end()) {
        cache.erase(it);
        LOG_INFO("CoroutineResourceManager: Unloaded resource: {}", normalized_path);
    } else {
        LOG_WARN("CoroutineResourceManager: Tried to unload non-existent resource: {}", normalized_path);
    }
}

template<typename T>
void CoroutineResourceManager::clear_cache() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    auto& cache = get_cache<T>();
    size_t count = cache.size();
    cache.clear();
    
    LOG_INFO("CoroutineResourceManager: Cleared {} cached resources of type {}",
                              count, typeid(T).name());
}

template<typename T>
std::vector<std::string> CoroutineResourceManager::get_cached_resource_names() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    const auto& cache = get_cache<T>();
    
    std::vector<std::string> names;
    names.reserve(cache.size());
    
    for (const auto& pair : cache) {
        names.push_back(pair.first);
    }
    
    return names;
}

// Dual-cache helper method implementations

template<typename T>
std::shared_ptr<T> CoroutineResourceManager::check_resource_cache(const std::string& normalized_path) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    const auto& cache = get_cache<T>();
    auto it = cache.find(normalized_path);
    if (it != cache.end()) {
        LOG_DEBUG("CoroutineResourceManager: Resource cache hit for: {}", normalized_path);
        return it->second;
    }
    return nullptr;
}

template<typename T>
std::shared_ptr<Async::Task<std::shared_ptr<T>>> CoroutineResourceManager::check_task_cache(const std::string& normalized_path) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    const auto& task_cache = get_task_cache<T>();
    auto it = task_cache.find(normalized_path);
    if (it != task_cache.end()) {
        LOG_DEBUG("CoroutineResourceManager: Task cache hit for: {}", normalized_path);
        return it->second;
    }
    return nullptr;
}

template<typename T>
void CoroutineResourceManager::cache_task(const std::string& normalized_path, std::shared_ptr<Async::Task<std::shared_ptr<T>>> task) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    auto& task_cache = get_task_cache<T>();
    task_cache[normalized_path] = task;
            LOG_DEBUG("CoroutineResourceManager: Cached loading task for: {}", normalized_path);
}

template<typename T>
void CoroutineResourceManager::cleanup_task_cache(const std::string& normalized_path) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    auto& task_cache = get_task_cache<T>();
    auto it = task_cache.find(normalized_path);
    if (it != task_cache.end()) {
        task_cache.erase(it);
        LOG_DEBUG("CoroutineResourceManager: Cleaned up task cache for: {}", normalized_path);
    }
}
