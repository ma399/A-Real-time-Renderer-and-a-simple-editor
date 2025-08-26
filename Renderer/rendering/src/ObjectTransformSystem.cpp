#include "ObjectTransformSystem.h"
#include "Camera.h"
#include "Scene.h"
#include "CoroutineResourceManager.h"
#include <Logger.h>
#include <GLFW/glfw3.h>

// Static member definition
const Transform ObjectTransformSystem::identity_transform_ = Transform::identity();

ObjectTransformSystem::ObjectTransformSystem()
    : current_mode_(TransformMode::kTranslate),
      drag_sensitivity_(1.0f),
      snap_to_grid_(false),
      grid_size_(1.0f) {
    LOG_INFO("ObjectTransformSystem: Initialized");
}

ObjectTransformSystem::~ObjectTransformSystem() = default;

bool ObjectTransformSystem::initialize(std::shared_ptr<RaycastSystem> raycast_system) {
    if (!raycast_system) {
        LOG_ERROR("ObjectTransformSystem: Invalid raycast system");
        return false;
    }

    raycast_system_ = raycast_system;
    LOG_INFO("ObjectTransformSystem: Initialized with raycast system");
    return true;
}

bool ObjectTransformSystem::start_drag(float screen_x, float screen_y,
                                      float screen_width, float screen_height,
                                      const Camera& camera,
                                      const Scene& scene,
                                      CoroutineResourceManager& resource_manager) {
    if (is_dragging()) {
        LOG_WARN("ObjectTransformSystem: Already dragging, ending previous drag");
        end_drag();
    }

    if (!raycast_system_) {
        LOG_ERROR("ObjectTransformSystem: Raycast system not initialized");
        return false;
    }

    // Generate ray from screen coordinates
    Ray ray = RaycastSystem::screen_to_world_ray(screen_x, screen_y, 
                                                screen_width, screen_height, camera);

    // Create callback to get model transforms
    auto transform_callback = [this](const std::string& model_id) -> glm::mat4 {
        // First check if we have a stored transform for this model
        auto it = transforms_.find(model_id);
        if (it != transforms_.end()) {
            return it->second.get_model_matrix();
        }
        
        // If no stored transform exists, create a default one and store it
        Transform default_transform = Transform::identity();
        
        // Set initial positions for known models
        if (model_id == "simple_scene_cube_model") {
            // Cube starts at origin
            default_transform.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
        } else if (model_id == "simple_scene_plane_model") {
            // Plane starts at Y = -1.0 (matching the mesh data)
            default_transform.set_position(glm::vec3(0.0f, -1.0f, 0.0f));
        }
        
        // Store the default transform for future use
        transforms_[model_id] = default_transform;
        
        LOG_DEBUG("ObjectTransformSystem: Created default transform for model '{}' at position ({:.2f}, {:.2f}, {:.2f})",
                 model_id, default_transform.get_position().x, default_transform.get_position().y, default_transform.get_position().z);
        
        return default_transform.get_model_matrix();
    };

    // Perform raycast to find object
    RaycastHit hit = raycast_system_->raycast(ray, scene, resource_manager, transform_callback);

    if (!hit.hit) {
        LOG_DEBUG("ObjectTransformSystem: No object hit at screen ({}, {})", screen_x, screen_y);
        return false;
    }

    // Initialize drag info
    drag_info_.model_id = hit.model_id;
    drag_info_.initial_hit_point = hit.point;
    drag_info_.initial_screen_x = screen_x;
    drag_info_.initial_screen_y = screen_y;
    drag_info_.mode = current_mode_;
    drag_info_.state = DragState::kStarting;

    // Get or create transform for the model
    Transform& transform = get_transform(hit.model_id);
    drag_info_.initial_model_position = transform.get_position();
    
    // Calculate offset from model center to hit point
    drag_info_.drag_offset = hit.point - drag_info_.initial_model_position;
    drag_info_.current_world_position = hit.point;

    // Transition to dragging state
    drag_info_.state = DragState::kDragging;

    LOG_INFO("ObjectTransformSystem: Started dragging model '{}' at ({:.2f}, {:.2f}, {:.2f})",
             hit.model_id, hit.point.x, hit.point.y, hit.point.z);

    return true;
}

bool ObjectTransformSystem::update_drag(float screen_x, float screen_y,
                                       float screen_width, float screen_height,
                                       const Camera& camera) {
    if (!is_dragging()) {
        return false;
    }

    // Calculate new world position based on screen movement
    glm::vec3 new_world_pos = calculate_drag_world_position(screen_x, screen_y,
                                                           screen_width, screen_height, camera);

    // Apply constraints if enabled
    new_world_pos = apply_constraints(new_world_pos);

    // Update drag info
    drag_info_.current_world_position = new_world_pos;

    // Update model transform based on mode
    Transform& transform = get_transform(drag_info_.model_id);
    
    switch (drag_info_.mode) {
        case TransformMode::kTranslate: {
            // Calculate new model position
            glm::vec3 new_model_pos = new_world_pos - drag_info_.drag_offset;
            transform.set_position(new_model_pos);
            break;
        }
        case TransformMode::kRotate: {
            // TODO: Implement rotation based on screen movement
            LOG_DEBUG("ObjectTransformSystem: Rotation mode not yet implemented");
            break;
        }
        case TransformMode::kScale: {
            // TODO: Implement scaling based on screen movement
            LOG_DEBUG("ObjectTransformSystem: Scale mode not yet implemented");
            break;
        }
    }

    LOG_DEBUG("ObjectTransformSystem: Updated drag position to ({:.2f}, {:.2f}, {:.2f})",
              new_world_pos.x, new_world_pos.y, new_world_pos.z);

    return true;
}

bool ObjectTransformSystem::end_drag() {
    if (!is_dragging()) {
        return false;
    }

    LOG_INFO("ObjectTransformSystem: Ended drag for model '{}'", drag_info_.model_id);
    
    drag_info_.state = DragState::kEnding;
    
    drag_info_.reset();
    
    return true;
}

Transform& ObjectTransformSystem::get_transform(const std::string& model_id) {
    auto it = transforms_.find(model_id);
    if (it == transforms_.end()) {
        // Create new identity transform
        transforms_[model_id] = Transform::identity();
        LOG_DEBUG("ObjectTransformSystem: Created new transform for model '{}'", model_id);
    }
    return transforms_[model_id];
}

const Transform& ObjectTransformSystem::get_transform(const std::string& model_id) const {
    auto it = transforms_.find(model_id);
    if (it != transforms_.end()) {
        return it->second;
    }
    return identity_transform_;
}

void ObjectTransformSystem::set_transform(const std::string& model_id, const Transform& transform) {
    transforms_[model_id] = transform;
    LOG_DEBUG("ObjectTransformSystem: Set transform for model '{}'", model_id);
}

void ObjectTransformSystem::remove_transform(const std::string& model_id) {
    auto it = transforms_.find(model_id);
    if (it != transforms_.end()) {
        transforms_.erase(it);
        LOG_DEBUG("ObjectTransformSystem: Removed transform for model '{}'", model_id);
    }
}

void ObjectTransformSystem::clear_transforms() {
    transforms_.clear();
    LOG_INFO("ObjectTransformSystem: Cleared all transforms");
}

glm::mat4 ObjectTransformSystem::get_model_matrix(const std::string& model_id) const {
    const Transform& transform = get_transform(model_id);
    return transform.get_model_matrix();
}

glm::vec3 ObjectTransformSystem::calculate_drag_world_position(float screen_x, float screen_y,
                                                             float screen_width, float screen_height,
                                                             const Camera& camera) const {
    
    // Generate ray for current screen position
    Ray current_ray = RaycastSystem::screen_to_world_ray(screen_x, screen_y,
                                                        screen_width, screen_height, camera);
    
    // Calculate distance from camera to initial hit point
    float hit_distance = glm::length(drag_info_.initial_hit_point - camera.get_position());
    
    // Project the ray to the same distance as the initial hit
    glm::vec3 projected_point = current_ray.origin + current_ray.direction * hit_distance;
    
    // Apply drag sensitivity
    glm::vec3 movement = (projected_point - drag_info_.initial_hit_point) * drag_sensitivity_;
    
    return drag_info_.initial_hit_point + movement;
}

glm::vec3 ObjectTransformSystem::apply_constraints(const glm::vec3& position) const {
    if (!snap_to_grid_) {
        return position;
    }
    
    // Snap to grid
    glm::vec3 snapped = position;
    snapped.x = std::round(snapped.x / grid_size_) * grid_size_;
    snapped.y = std::round(snapped.y / grid_size_) * grid_size_;
    snapped.z = std::round(snapped.z / grid_size_) * grid_size_;
    
    return snapped;
}

void ObjectTransformSystem::set_rotation_animation(const std::string& model_id, bool enable, const glm::vec3& rotation_speed) {
    AnimationData& anim_data = animations_[model_id];
    anim_data.enabled = enable;
    anim_data.rotation_speed = rotation_speed;
    
    // Store base transform data if enabling animation
    if (enable) {
        const Transform& transform = get_transform(model_id);
        anim_data.base_position = transform.get_position();
        anim_data.base_scale = transform.get_scale();
    }
    
    LOG_DEBUG("ObjectTransformSystem: {} rotation animation for model '{}'", 
              enable ? "Enabled" : "Disabled", model_id);
}

void ObjectTransformSystem::update_animations(float current_time) {
    for (auto& [model_id, anim_data] : animations_) {
        if (!anim_data.enabled) {
            continue;
        }
        
        // Get the transform for this model
        Transform& transform = get_transform(model_id);
        
        // Apply time-based rotation
        float rotation_x = current_time * anim_data.rotation_speed.x;
        float rotation_y = current_time * anim_data.rotation_speed.y;
        float rotation_z = current_time * anim_data.rotation_speed.z;
        
        // Set the animated rotation while preserving base position and scale
        transform.set_position(anim_data.base_position);
        transform.set_rotation(rotation_x, rotation_y, rotation_z);
        transform.set_scale(anim_data.base_scale);
        
    }
}

bool ObjectTransformSystem::has_animation(const std::string& model_id) const {
    auto it = animations_.find(model_id);
    return it != animations_.end() && it->second.enabled;
}
