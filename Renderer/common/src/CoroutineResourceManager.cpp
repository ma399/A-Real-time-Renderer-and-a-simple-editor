#include "CoroutineResourceManager.h"
#include "AssimpLoader.h"
#include "Logger.h"
#include "Shader.h"
#include "Scene.h"
#include "Light.h"
#include <filesystem>
#include <shared_mutex>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

CoroutineResourceManager::CoroutineResourceManager() 
    : scheduler_(&Async::CoroutineThreadPoolScheduler::get_instance()) {

    LOG_DEBUG("CoroutineResourceManager: Constructor STARTED");
    LOG_DEBUG("CoroutineResourceManager: scheduler_ pointer = {}", (void*)scheduler_);
    
    // Verify the scheduler is valid and running
    if (scheduler_ == nullptr) {
        LOG_ERROR("CoroutineResourceManager: Scheduler instance is null!");
        throw std::runtime_error("Scheduler instance is null");
    }
    
    if (!scheduler_->is_running()) {
        LOG_ERROR("CoroutineResourceManager: Scheduler is not running!");
        throw std::runtime_error("Scheduler is not running");
    }
    
    LOG_INFO("CoroutineResourceManager: Successfully connected to CoroutineThreadPoolScheduler (running: {})", 
             scheduler_->is_running());
    
    // Initialize AssimpLoader directly
    assimp_loader_ = std::make_unique<AssimpLoader>();
}

CoroutineResourceManager::~CoroutineResourceManager() {
    LOG_INFO("CoroutineResourceManager: Shutting down resource management system");
    
    size_t total_cached_resources = get_cache_size();
    if (total_cached_resources > 0) {
        LOG_INFO("CoroutineResourceManager: Clearing {} cached resources", total_cached_resources);
    }
    
    clear_all_caches();
    LOG_INFO("CoroutineResourceManager: Shutdown complete");
}

Async::Task<std::shared_ptr<Mesh>> CoroutineResourceManager::load_mesh_async(const std::string& path,
                                                                            std::function<void(float, const std::string&)> progressCallback,
                                                                            Async::TaskPriority priority) {  
    std::string normalized_path = normalize_resource_path(path);
    LOG_INFO("Normalized path: '{}'", normalized_path);
    
    {
        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);
        auto it = mesh_cache_.find(normalized_path);
        if (it != mesh_cache_.end()) {
            update_stats(priority, true);
            if (progressCallback) {
                progressCallback(1.0f, "Loaded from cache");
            }
            LOG_DEBUG("CoroutineResourceManager: Found cached mesh with progress: {}", normalized_path);
            co_return it->second;
        }
    }
    
    update_stats(priority, false);
    
    if (!validate_resource_path(path)) {
        LOG_ERROR("CoroutineResourceManager: Invalid path for mesh load: {}", path);
        if (progressCallback) {
            progressCallback(0.0f, "Invalid path");
        }
        co_return nullptr;
    }
    
    try {
        // Check if scheduler is available
        if (!scheduler_) {
            LOG_ERROR("CoroutineResourceManager: Scheduler not available for mesh loading (scheduler_ is null)");
            if (progressCallback) {
                progressCallback(0.0f, "Scheduler not available");
            }
            co_return nullptr;
        }
        
        // Additional check for scheduler state
        if (!scheduler_->is_running()) {
            LOG_ERROR("CoroutineResourceManager: Scheduler is not running for mesh loading");
            if (progressCallback) {
                progressCallback(0.0f, "Scheduler not running");
            }
            co_return nullptr;
        }
        
        LOG_DEBUG("CoroutineResourceManager: Scheduler is available and running, proceeding with mesh loading");
        
        // Use AssimpLoader directly in thread pool
        auto mesh = co_await scheduler_->submit_to_threadpool(priority, [this, path, progressCallback]() -> std::shared_ptr<Mesh> {
            if (progressCallback) {
                progressCallback(0.1f, "Starting file load...");
            }

            std::vector<Mesh::Vertex> vertices;
            std::vector<Mesh::Indices> indices;

            if (progressCallback) {
                progressCallback(0.3f, "Loading model data...");
            }

            // Direct AssimpLoader call
            assimp_loader_->load_model(path, vertices, indices);

            if (progressCallback) {
                progressCallback(0.8f, "Creating mesh...");
            }

            auto mesh = std::make_shared<Mesh>(vertices, indices);

            if (progressCallback) {
                progressCallback(1.0f, "Completed!");
            }
           
            LOG_INFO("CoroutineResourceManager: Loaded {} vertices, {} indices", vertices.size(), indices.size());
            
            return mesh;
        });
        
        if (mesh) {
            // cache the loaded mesh
            {
                std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
                mesh_cache_[normalized_path] = mesh;
                LOG_DEBUG("CoroutineResourceManager: Cached mesh: {}", normalized_path);
            }
            
            stats_.async_loads_completed.fetch_add(1, std::memory_order_relaxed);
        }
        
        co_return mesh;
        
    } catch (const std::exception& e) {
        LOG_ERROR("CoroutineResourceManager: Exception during mesh load for {}: {}", path, e.what());
        if (progressCallback) {
            progressCallback(0.0f, "Load failed: " + std::string(e.what()));
        }
        co_return nullptr;
    }
}

// Texture loading
Async::Task<std::shared_ptr<Texture>> CoroutineResourceManager::load_texture_async(const std::string& path, Async::TaskPriority priority) {
    LOG_INFO("CoroutineResourceManager: Starting coroutine texture load for: {}", path);
    
    std::string normalized_path = normalize_resource_path(path);

    // Check cache
    {
        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);
        auto it = texture_cache_.find(normalized_path);
        if (it != texture_cache_.end()) {
            update_stats(priority, true);
            LOG_DEBUG("CoroutineResourceManager: Found cached texture: {}", normalized_path);
            co_return it->second;
        }
    }
    
    update_stats(priority, false);
    
    if (!validate_resource_path(path)) {
        LOG_ERROR("CoroutineResourceManager: Invalid path for texture load: {}", path);
        co_return nullptr;
    }
    
    try {
        LOG_DEBUG("CoroutineResourceManager: Loading texture from disk: {}", path);
        
        // Check if scheduler is available
        if (!scheduler_) {
            LOG_ERROR("CoroutineResourceManager: Scheduler not available for texture loading");
            co_return nullptr;
        }
        
        // Use coroutine scheduler to submit to thread pool
        auto texture = co_await scheduler_->submit_to_threadpool(priority, [path]() -> std::shared_ptr<Texture> {
            LOG_DEBUG("CoroutineResourceManager: Worker thread loading texture: {}", path);
            
            // This should be the actual texture loading logic
            auto texture = std::make_shared<Texture>();
            
            LOG_DEBUG("CoroutineResourceManager: Texture loaded successfully: {}", path);
            return texture;
        });
        
        if (texture) {
            // Cache result
            {
                std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
                texture_cache_[normalized_path] = texture;
                LOG_DEBUG("CoroutineResourceManager: Cached texture: {}", normalized_path);
            }
            
            stats_.async_loads_completed.fetch_add(1, std::memory_order_relaxed);
        }
        
        LOG_INFO("CoroutineResourceManager: Coroutine texture load completed for: {}", path);
        co_return texture;
        
    } catch (const std::exception& e) {
        LOG_ERROR("CoroutineResourceManager: Exception during texture load for {}: {}", path, e.what());
        co_return nullptr;
    }
}

CoroutineResourceManager::StatsObserver CoroutineResourceManager::get_stats() const {
    StatsObserver observer;
    observer.total_loads = stats_.total_loads.load(std::memory_order_relaxed);
    observer.task_cache_hits = stats_.task_cache_hits.load(std::memory_order_relaxed);
    observer.task_cache_misses = stats_.task_cache_misses.load(std::memory_order_relaxed);
    observer.async_loads_requested = stats_.async_loads_requested.load(std::memory_order_relaxed);
    observer.async_loads_completed = stats_.async_loads_completed.load(std::memory_order_relaxed);
    observer.duplicate_requests_avoided = stats_.duplicate_requests_avoided.load(std::memory_order_relaxed);

    observer.priority_loads[0] = stats_.priority_loads[0].load(std::memory_order_relaxed); // background
    observer.priority_loads[1] = stats_.priority_loads[1].load(std::memory_order_relaxed); // normal
    observer.priority_loads[2] = stats_.priority_loads[2].load(std::memory_order_relaxed); // high
    observer.priority_loads[3] = stats_.priority_loads[3].load(std::memory_order_relaxed); // critical
    return observer;
}

void CoroutineResourceManager::reset_stats() {
    stats_.total_loads.store(0, std::memory_order_relaxed);
    stats_.task_cache_hits.store(0, std::memory_order_relaxed);
    stats_.task_cache_misses.store(0, std::memory_order_relaxed);
    stats_.async_loads_requested.store(0, std::memory_order_relaxed);
    stats_.async_loads_completed.store(0, std::memory_order_relaxed);
    stats_.duplicate_requests_avoided.store(0, std::memory_order_relaxed);
    
    stats_.priority_loads[0].store(0, std::memory_order_relaxed); // background
    stats_.priority_loads[1].store(0, std::memory_order_relaxed); // normal
    stats_.priority_loads[2].store(0, std::memory_order_relaxed); // high
    stats_.priority_loads[3].store(0, std::memory_order_relaxed); // critical

    LOG_INFO("CoroutineResourceManager: Statistics reset (including dual-cache metrics)");
}

void CoroutineResourceManager::clear_all_caches() {
    // Clear both resource cache and task cache with single lock
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);

    // Clear resource caches
    mesh_cache_.clear();
    texture_cache_.clear();
    material_cache_.clear();
    model_cache_.clear();
    irradiance_cache_.clear();
    
    // Clear task caches
    mesh_task_cache_.clear();
    texture_task_cache_.clear();
    material_task_cache_.clear();
    model_task_cache_.clear();
    
    LOG_INFO("CoroutineResourceManager: Cleared all caches");
}

size_t CoroutineResourceManager::get_cache_size() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return mesh_cache_.size() + texture_cache_.size() + material_cache_.size() + model_cache_.size() + irradiance_cache_.size();
}

bool CoroutineResourceManager::validate_resource_path(const std::string& path) const {
    LOG_INFO("Validating path: '{}'", path);
    
    if (path.empty()) {
        LOG_ERROR("Path is empty");
        return false;
    }
    
    try {
        std::filesystem::path fsPath(path);
        LOG_INFO("Filesystem path: '{}'", fsPath.string());
        
        bool exists = std::filesystem::exists(fsPath);     
        if (!exists) {
            LOG_ERROR("File does not exist: {}", path);
            return false;
        }
        
        LOG_INFO("Path validation successful: {}", path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Path validation exception for {}: {}", path, e.what());
        return false;
    }
}

std::string CoroutineResourceManager::normalize_resource_path(const std::string& path) const {
    try {
        std::filesystem::path fsPath(path);
        return fsPath.lexically_normal().string();
    } catch (const std::exception& e) {
        LOG_WARN("CoroutineResourceManager: Path normalization failed for {}: {}", path, e.what());
        return path;
    }
}

void CoroutineResourceManager::update_stats(Async::TaskPriority priority, bool cache_hit) const {
    stats_.total_loads.fetch_add(1, std::memory_order_relaxed);
    
    if (cache_hit) {
        stats_.task_cache_hits.fetch_add(1, std::memory_order_relaxed);
    } else {
        stats_.task_cache_misses.fetch_add(1, std::memory_order_relaxed);
    }
    
    switch (priority) {
        case Async::TaskPriority::k_background:
            stats_.priority_loads[0].fetch_add(1, std::memory_order_relaxed);
            break;
        case Async::TaskPriority::k_normal:
            stats_.priority_loads[1].fetch_add(1, std::memory_order_relaxed);
            break;
        case Async::TaskPriority::k_high:
            stats_.priority_loads[2].fetch_add(1, std::memory_order_relaxed);
            break;
        case Async::TaskPriority::k_critical:
            stats_.priority_loads[3].fetch_add(1, std::memory_order_relaxed);
            break;
    }
}

std::shared_ptr<Model> CoroutineResourceManager::assemble_model(const std::string& mesh_path, const std::string& material_path) {
    
    // Load mesh
    auto mesh = get<Mesh>(mesh_path);
    if (!mesh) {
        LOG_ERROR("CoroutineResourceManager: Failed to load mesh from path: {}", mesh_path);
        return nullptr;
    }
    
    // Load material
    auto material = get<Material>(material_path);
    if (!material) {
        LOG_ERROR("CoroutineResourceManager: Failed to load material from path: {}", material_path);
        return nullptr;
    }
    
    // Create model 
    auto model = std::make_shared<Model>(mesh.get(), material.get());

    return model;
}

std::shared_ptr<Model> CoroutineResourceManager::assumble_model(const Mesh& mesh, const Material& material) {
  auto model = std::make_shared<Model>(&mesh, &material);
  return model;
}

std::shared_ptr<Texture> CoroutineResourceManager::get_material_texture(const Material& material, const std::string& texture_name) {
    LOG_DEBUG("CoroutineResourceManager: Getting material texture '{}'", texture_name);
    
    std::string texture_path = material.get_texture_path(texture_name);
    if (texture_path.empty()) {
        LOG_DEBUG("CoroutineResourceManager: No texture path found for '{}'", texture_name);
        return nullptr;
    }
    
    auto texture = get<Texture>(texture_path);
    if (!texture) {
        LOG_WARN("CoroutineResourceManager: Failed to load texture from path: {}", texture_path);
    }
    
    return texture;
}

void CoroutineResourceManager::set_material_texture(Material& material, const std::string& texture_name, const std::string& texture_path) {
    LOG_DEBUG("CoroutineResourceManager: Setting material texture '{}' to path '{}'", 
                               texture_name, texture_path);
    
    // Set texture path in material
    material.add_texture(texture_name, texture_path);
    
    // Preload the texture
    auto texture = get<Texture>(texture_path);
    if (!texture) {
        LOG_WARN("CoroutineResourceManager: Failed to preload texture from path: {}", texture_path);
    } else {
        LOG_DEBUG("CoroutineResourceManager: Texture preloaded successfully");
    }
}

std::unordered_map<std::string, std::shared_ptr<Texture>> CoroutineResourceManager::get_material_textures(const Material& material) {
    
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    
    const auto& texturePaths = material.get_all_texture_paths();
    for (const auto& [name, path] : texturePaths) {
        auto texture = get<Texture>(path);
        if (texture) {
            textures[name] = texture;
            LOG_DEBUG("CoroutineResourceManager: Loaded texture '{}' from path '{}'", name, path);
        } else {
            LOG_WARN("CoroutineResourceManager: Failed to load texture '{}' from path '{}'", name, path);
        }
    }
    
    return textures;
}

std::vector<std::shared_ptr<Model>> CoroutineResourceManager::get_scene_models(const Scene& scene) {
    std::vector<std::shared_ptr<Model>> models;
    
    const auto& modelRefs = scene.get_model_references();
    models.reserve(modelRefs.size());
    
    for (const auto& modelId : modelRefs) {
        auto model = get<Model>(modelId);
        if (model) {
            models.push_back(model);
        } else {
            LOG_WARN("CoroutineResourceManager: Model '{}' not found in cache", modelId);
        }
    }
    
    return models;
}

std::vector<std::shared_ptr<Light>> CoroutineResourceManager::get_scene_lights(const Scene& scene) const {
    std::vector<std::shared_ptr<Light>> lights;
    
    const auto& lightRefs = scene.get_light_references();
    lights.reserve(lightRefs.size());
    
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    for (const auto& light_id : lightRefs) {
        auto it = light_cache_.find(light_id);
        if (it != light_cache_.end()) {
            lights.push_back(it->second);
        } else {
            LOG_WARN("CoroutineResourceManager: Light '{}' not found in cache", light_id);
        }
    }
    
    //LOG_DEBUG("CoroutineResourceManager: Retrieved {}/{} lights from scene", lights.size(), lightRefs.size());
    return lights;
}

void CoroutineResourceManager::store_light_in_cache(const std::string& light_id, std::shared_ptr<Light> light) {
    if (!light) {
        LOG_ERROR("CoroutineResourceManager: Invalid light pointer for '{}'", light_id);
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    light_cache_[light_id] = light;
    LOG_DEBUG("CoroutineResourceManager: Light '{}' stored in cache", light_id);
}

std::shared_ptr<Shader> CoroutineResourceManager::create_shader(const std::string& shaderName,
                                                                const std::string& vertexPath,
                                                                const std::string& fragmentPath,
                                                                const std::string& geometryPath,
                                                                const std::string& computePath) {
    LOG_INFO("CoroutineResourceManager: Creating shader '{}' from vertex '{}' and fragment '{}'", 
                              shaderName, vertexPath, fragmentPath);
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    // Check if shader already exists
    auto it = shader_cache_.find(shaderName);
    if (it != shader_cache_.end()) {
        LOG_WARN("CoroutineResourceManager: Shader '{}' already exists, returning existing", shaderName);
        return it->second;
    }
    
    try {
        // Create new shader
        std::shared_ptr<Shader> shader;
        if (!computePath.empty()) {
            // Compute shader
            shader = std::make_shared<Shader>(vertexPath.c_str(), fragmentPath.c_str(), geometryPath.c_str(), computePath.c_str());
        } else if (geometryPath.empty()) {
            shader = std::make_shared<Shader>(vertexPath.c_str(), fragmentPath.c_str());
        } else {
            shader = std::make_shared<Shader>(vertexPath.c_str(), fragmentPath.c_str(), geometryPath.c_str());
        }
        
        // Store in cache
        shader_cache_[shaderName] = shader;
        
        LOG_INFO("CoroutineResourceManager: Shader '{}' created and cached successfully", shaderName);
        return shader;
        
    } catch (const std::exception& e) {
        LOG_ERROR("CoroutineResourceManager: Failed to create shader '{}': {}", shaderName, e.what());
        return nullptr;
    }
}

std::shared_ptr<Shader> CoroutineResourceManager::get_shader(const std::string& shaderName) const{
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = shader_cache_.find(shaderName);
    if (it != shader_cache_.end()) {
        return it->second;
    }
    
    LOG_WARN("CoroutineResourceManager: Shader '{}' not found in cache", shaderName);
    return nullptr;
}

void CoroutineResourceManager::store_shader(const std::string& shaderName, std::shared_ptr<Shader> shader) {
    if (!shader) {
        LOG_ERROR("CoroutineResourceManager: Invalid shader pointer for '{}'", shaderName);
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    shader_cache_[shaderName] = shader;
    LOG_DEBUG("CoroutineResourceManager: Shader '{}' stored in cache", shaderName);
}

void CoroutineResourceManager::remove_shader(const std::string& shaderName) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = shader_cache_.find(shaderName);
    if (it != shader_cache_.end()) {
        shader_cache_.erase(it);
        LOG_INFO("CoroutineResourceManager: Shader '{}' removed from cache", shaderName);
    } else {
        LOG_WARN("CoroutineResourceManager: Tried to remove non-existent shader '{}'", shaderName);
    }
}

std::vector<std::string> CoroutineResourceManager::get_shader_names() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    std::vector<std::string> names;
    names.reserve(shader_cache_.size());
    
    for (const auto& [name, shader] : shader_cache_) {
        names.push_back(name);
    }
    
    LOG_DEBUG("CoroutineResourceManager: Retrieved {} shader names", names.size());
    return names;
}

std::unique_ptr<Scene> CoroutineResourceManager::create_simple_scene() {
    LOG_INFO("CoroutineResourceManager: Creating simple scene");
    
    // Create scene
    auto scene = std::make_unique<Scene>();
    
    // Create cube mesh data
    std::vector<Mesh::Vertex> vertices = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

        // Back face
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

        // Left face
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

        // Right face
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},

        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

        // Top face
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}
    };

    std::vector<unsigned int> indices = {
        0,  1,  2,    2,  3,  0,   // front
        4,  5,  6,    6,  7,  4,   // back
        8,  9,  10,   10, 11, 8,   // left
        12, 13, 14,   14, 15, 12,  // right
        16, 17, 18,   18, 19, 16,  // bottom
        20, 21, 22,   22, 23, 20   // top
    };

    // Create cube mesh and store in cache
    auto cube_mesh = std::make_shared<Mesh>(vertices, indices);
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        mesh_cache_["simple_scene_cube"] = cube_mesh;
    }


    
    // Create plane mesh data
    std::vector<Mesh::Vertex> plane_vertices = {
        // Plane 
        {{-100.0f, -1.2f, -100.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // bottom-left
        {{ 100.0f, -1.2f, -100.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // bottom-right
        {{ 100.0f, -1.2f,  100.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},  // top-right
        {{-100.0f, -1.2f,  100.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}}   // top-left
    };

    std::vector<unsigned int> plane_indices = {
        // Front-facing triangles (clockwise from above)
        0, 1, 2,   // first triangle
        0, 2, 3,   // second triangle
        // Back-facing triangles (counter-clockwise from above) 
        0, 2, 1,   // first triangle (reversed)
        0, 3, 2    // second triangle (reversed)
    };

    // Create plane mesh and store in cache
    auto plane_mesh = std::make_shared<Mesh>(plane_vertices, plane_indices);
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        mesh_cache_["simple_scene_plane"] = plane_mesh;
    }
   
    // Main directional light for shadow casting 
    auto directional_light_main = std::make_shared<DirectionalLight>(
        glm::vec3(-0.5f, -1.0f, -0.5f),  // Direction from top-right to bottom
        glm::vec3(1.0f, 1.0f, 1.0f)      
    );
    directional_light_main->set_intensity(1.5f);
    store_light_in_cache("directional_light_main", directional_light_main);
    
    // Warm point light from the right 
    auto point_light_1 = std::make_shared<PointLight>(
        glm::vec3(1.2f, 1.2f, 0.8f),   
        glm::vec3(1.0f, 0.8f, 0.6f),   
        8.0f  
    );
    point_light_1->set_intensity(1.5f);  
    store_light_in_cache("point_light_1", point_light_1);
    
    // Cool light from the left
    auto point_light_2 = std::make_shared<PointLight>(
        glm::vec3(-1.2f, 1.5f, 1.0f),  
        glm::vec3(0.6f, 0.8f, 1.0f),   
        8.0f  // Larger range for better coverage
    );
    point_light_2->set_intensity(2.0f);  
    store_light_in_cache("point_light_2", point_light_2);
    
    // Spot Light 1 
    auto spot_light_1 = std::make_shared<SpotLight>(
        glm::vec3(0.0f, 2.0f, 1.5f),   
        glm::vec3(0.0f, -0.8f, -1.0f), 
        glm::vec3(1.0f, 1.0f, 0.9f),   // White 
        25.0f,  
        35.0f   
    );
    spot_light_1->set_intensity(3.0f);  
    store_light_in_cache("spot_light_1", spot_light_1);
    
    // Spot Light 2 
    auto spot_light_2 = std::make_shared<SpotLight>(
        glm::vec3(0.8f, 2.0f, -1.5f),  
        glm::vec3(-0.5f, -0.5f, 1.0f), 
        glm::vec3(0.9f, 0.7f, 1.0f),   // Purple 
        25.0f,  
        40.0f   
    );
    spot_light_2->set_intensity(0.6f);
    store_light_in_cache("spot_light_2", spot_light_2);
    
    // Directional light
    auto directional_light = std::make_shared<DirectionalLight>(
        glm::vec3(-0.3f, -1.0f, -0.2f), // Direction 
        glm::vec3(1.0f, 0.95f, 0.9f)    
    );
    directional_light->set_intensity(1.2f);  
    store_light_in_cache("directional_light", directional_light);

    // Create materials using predefined presets
    // Cube: Diffuse material (wood-like)
    auto cube_material = std::make_shared<Material>(Material::create_pbr_wood());
    cube_material->set_albedo(glm::vec3(0.8f, 0.4f, 0.2f)); // Warm wood color
    cube_material->set_diffuse(glm::vec3(0.8f, 0.4f, 0.2f)); // Match diffuse to albedo
    cube_material->set_ambient(glm::vec3(0.12f, 0.06f, 0.03f)); // Darker ambient
    
    // Plane: Glossy material (metallic)
    auto plane_material = std::make_shared<Material>(Material::create_pbr_metal());
    plane_material->set_albedo(glm::vec3(0.7f, 0.7f, 0.8f)); // Slightly blue metallic
    plane_material->set_diffuse(glm::vec3(0.7f, 0.7f, 0.8f)); // Match diffuse to albedo
    plane_material->set_roughness(0.1f); // Very smooth for glossy appearance
    plane_material->set_ambient(glm::vec3(0.1f, 0.1f, 0.12f)); // Slightly blue ambient

    // Store materials in cache
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        material_cache_["simple_scene_cube_material"] = cube_material;
        material_cache_["simple_scene_plane_material"] = plane_material;
    }

    // Create shaders
    auto main_shader = create_shader("simple_scene_main_shader",
                                   "../assets/shaders/vertex.glsl",
                                   "../assets/shaders/phong_fragment.glsl");
    
    auto light_shader = create_shader("simple_scene_light_shader",
                                    "../assets/shaders/light_vertex.glsl",
                                    "../assets/shaders/light_fragment.glsl");
    
    // Create deferred rendering shaders
    auto deferred_geometry_shader = create_shader("deferred_geometry_shader",
                                                "../assets/shaders/deferred_geometry_vertex.glsl",
                                                "../assets/shaders/deferred_geometry_fragment.glsl");
    
    auto deferred_lighting_shader = create_shader("deferred_lighting_shader",
                                                "../assets/shaders/deferred_lighting_vertex.glsl",
                                                "../assets/shaders/deferred_lighting_fragment.glsl");
    
    if (!deferred_lighting_shader) {
        LOG_ERROR("Failed to create deferred_lighting_shader!");
    } else {
        LOG_INFO("Successfully created deferred_lighting_shader");
    }
    
    // Create SSGI shaders
    auto deferred_lighting_direct_shader = create_shader("deferred_lighting_direct_shader",
                                                       "../assets/shaders/deferred_lighting_direct_vertex.glsl",
                                                       "../assets/shaders/deferred_lighting_direct_fragment.glsl");
    
    auto ssgi_compute_shader = create_shader("ssgi_compute_shader",
                                           "",
                                           "",
                                           "",
                                           "../assets/shaders/ssgi_compute.glsl");
    
    auto ssgi_denoise_shader = create_shader("ssgi_denoise_shader",
                                           "../assets/shaders/ssgi_denoise_vertex.glsl",
                                           "../assets/shaders/ssgi_denoise_fragment.glsl");
    
    auto ssgi_composition_shader = create_shader("ssgi_composition_shader",
                                               "../assets/shaders/ssgi_composition_vertex.glsl",
                                               "../assets/shaders/ssgi_composition_fragment.glsl");
    
    if (!deferred_lighting_direct_shader || !ssgi_compute_shader || !ssgi_denoise_shader || !ssgi_composition_shader) {
        LOG_ERROR("Failed to create SSGI shaders!");
    } else {
        LOG_INFO("Successfully created all SSGI shaders");
    }
    
    auto gbuffer_debug_shader = create_shader("gbuffer_debug_shader",
                                            "../assets/shaders/gbuffer_debug_vertex.glsl",
                                            "../assets/shaders/gbuffer_debug_fragment.glsl");
    
    // Create skybox shader
    auto skybox_shader = create_shader("skybox_shader",
                                     "../assets/shaders/skybox_vertex.glsl",
                                     "../assets/shaders/skybox_fragment.glsl");
    
    // Create plane reflection shader
    auto plane_reflection_shader = create_shader("plane_reflection_shader",
                                               "../assets/shaders/vertex.glsl",
                                               "../assets/shaders/plane_reflection_fragment.glsl");

    // Create models by combining meshes and materials (observers only)
    auto cube_model = std::make_shared<Model>(cube_mesh.get(), cube_material.get());
    
    auto plane_model = std::make_shared<Model>(plane_mesh.get(), plane_material.get());
    
    // Store models in cache
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        model_cache_["simple_scene_cube_model"] = cube_model;
        model_cache_["simple_scene_plane_model"] = plane_model;
    }

    assemble_model("simple_scene_cube_model", "simple_scene_cube_material");
    
    // Create skybox cubemap texture
    auto skybox_texture = std::make_shared<Texture>();
    std::vector<std::string> skybox_faces = {
        "../assets/textures/skybox/skybox/right.jpg",   // +X
        "../assets/textures/skybox/skybox/left.jpg",    // -X
        "../assets/textures/skybox/skybox/top.jpg",     // +Y
        "../assets/textures/skybox/skybox/bottom.jpg",  // -Y
        "../assets/textures/skybox/skybox/front.jpg",   // +Z
        "../assets/textures/skybox/skybox/back.jpg"     // -Z
    };
    skybox_texture->load_cubemap_from_files(skybox_faces);
    
    // Store skybox texture in cache
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        texture_cache_["skybox_cubemap"] = skybox_texture;
    }
    
    // Automatically compute irradiance map for the skybox
    LOG_INFO("CoroutineResourceManager: Computing irradiance map for skybox_cubemap");
    auto irradiance_map = compute_irradiance_map("skybox_cubemap", 32);
    if (irradiance_map) {
        LOG_INFO("CoroutineResourceManager: Successfully computed irradiance map");
    } else {
        LOG_ERROR("CoroutineResourceManager: Failed to compute irradiance map");
    }


    // Add references to scene 
    scene->add_model_reference("simple_scene_cube_model");
    scene->add_model_reference("simple_scene_plane_model");
    
    // Add all light references 
    scene->add_light_reference("directional_light_main");  // Main shadow casting light
    scene->add_light_reference("directional_light");       // Secondary directional light
    scene->add_light_reference("point_light_1");
    scene->add_light_reference("point_light_2");
    scene->add_light_reference("spot_light_1");
    scene->add_light_reference("spot_light_2");
    
    // Set ambient light for the scene 
    scene->set_ambient_light(glm::vec3(0.2f, 0.2f, 0.25f)); 

    return scene;
}

std::shared_ptr<Texture> CoroutineResourceManager::compute_irradiance_map(const std::string& skybox_texture_name, int irradiance_size) {
    LOG_INFO("CoroutineResourceManager: Computing irradiance map for skybox: {}", skybox_texture_name);
    
    // Check if irradiance map already exists
    std::string irradiance_key = skybox_texture_name + "_irradiance";
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        auto it = irradiance_cache_.find(irradiance_key);
        if (it != irradiance_cache_.end()) {
            LOG_DEBUG("CoroutineResourceManager: Found cached irradiance map: {}", irradiance_key);
            return it->second;
        }
    }
    
    // Get the skybox texture
    auto skybox_texture = get<Texture>(skybox_texture_name);
    if (!skybox_texture) {
        LOG_ERROR("CoroutineResourceManager: Skybox texture '{}' not found for irradiance computation", skybox_texture_name);
        return nullptr;
    }
    
    // Create or get irradiance shader
    auto irradiance_shader = get_shader("irradiance_shader");
    if (!irradiance_shader) {
        irradiance_shader = create_shader("irradiance_shader",
                                        "../assets/shaders/irradiance_vertex.glsl",
                                        "../assets/shaders/irradiance_fragment.glsl");
        if (!irradiance_shader) {
            LOG_ERROR("CoroutineResourceManager: Failed to create irradiance shader");
            return nullptr;
        }
    }
    
    // Create irradiance cubemap
    auto irradiance_map = std::make_shared<Texture>();
    GLuint irradiance_fbo, irradiance_rbo;
    
    // Setup framebuffer for irradiance computation
    glGenFramebuffers(1, &irradiance_fbo);
    glGenRenderbuffers(1, &irradiance_rbo);
    
    glBindFramebuffer(GL_FRAMEBUFFER, irradiance_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, irradiance_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradiance_size, irradiance_size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, irradiance_rbo);
    
    // Use the texture ID from the Texture object (it's already generated in constructor)
    GLuint irradiance_texture_id = irradiance_map->get_id();
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_texture_id);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 
                     irradiance_size, irradiance_size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Setup projection and view matrices for cubemap faces
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    
    // Setup cube geometry for rendering
    float cube_vertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    
    GLuint cube_vao, cube_vbo;
    glGenVertexArrays(1, &cube_vao);
    glGenBuffers(1, &cube_vbo);
    glBindVertexArray(cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    
    // Store current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Configure shader and render irradiance map
    irradiance_shader->use();
    irradiance_shader->set_int("environmentMap", 0);
    irradiance_shader->set_mat4("projection", captureProjection);
    
    // Bind skybox texture
    glActiveTexture(GL_TEXTURE0);
    skybox_texture->bind_cube_map(0);
    
    glViewport(0, 0, irradiance_size, irradiance_size);
    glBindFramebuffer(GL_FRAMEBUFFER, irradiance_fbo);
    
    // Render each face of the irradiance cubemap
    for (unsigned int i = 0; i < 6; ++i) {
        irradiance_shader->set_mat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradiance_texture_id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glBindVertexArray(cube_vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }
    
    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &irradiance_fbo);
    glDeleteRenderbuffers(1, &irradiance_rbo);
    glDeleteVertexArrays(1, &cube_vao);
    glDeleteBuffers(1, &cube_vbo);
    
    // Restore viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    
    // Store in cache
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        irradiance_cache_[irradiance_key] = irradiance_map;
    }
    
    LOG_INFO("CoroutineResourceManager: Successfully computed irradiance map for: {}", skybox_texture_name);
    return irradiance_map;
}

void CoroutineResourceManager::store_irradiance_map(const std::string& skybox_texture_name, std::shared_ptr<Texture> irradiance_map) {
    if (!irradiance_map) {
        LOG_ERROR("CoroutineResourceManager: Invalid irradiance map pointer for '{}'", skybox_texture_name);
        return;
    }
    
    std::string irradiance_key = skybox_texture_name + "_irradiance";
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    irradiance_cache_[irradiance_key] = irradiance_map;
    LOG_INFO("CoroutineResourceManager: Stored irradiance map for skybox: {}", skybox_texture_name);
}

std::shared_ptr<Texture> CoroutineResourceManager::get_irradiance_map(const std::string& skybox_texture_name) const {
    std::string irradiance_key = skybox_texture_name + "_irradiance";
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = irradiance_cache_.find(irradiance_key);
    if (it != irradiance_cache_.end()) {
        return it->second;
    }
    return nullptr;
}