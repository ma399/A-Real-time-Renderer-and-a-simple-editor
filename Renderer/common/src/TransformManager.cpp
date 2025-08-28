#include "TransformManager.h"
#include "Camera.h"
#include "Scene.h"
#include "CoroutineResourceManager.h"
#include <Logger.h>

// Static member definition
const Transform TransformManager::identity_transform_ = Transform::identity();

TransformManager::TransformManager() {
    LOG_INFO("TransformManager: Initialized");
}

Transform& TransformManager::get_transform(const std::string& model_id) {
    auto it = transforms_.find(model_id);
    if (it == transforms_.end()) {
        // Create new identity transform with default positions for known models
        Transform default_transform = Transform::identity();
        
        if (model_id == "simple_scene_cube_model") {
            default_transform.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
        } else if (model_id == "simple_scene_plane_model") {
            default_transform.set_position(glm::vec3(0.0f, -1.0f, 0.0f));
        }
        
        transforms_[model_id] = default_transform;
        LOG_DEBUG("TransformManager: Created new transform for model '{}'", model_id);
    }
    return transforms_[model_id];
}

const Transform& TransformManager::get_transform(const std::string& model_id) const {
    auto it = transforms_.find(model_id);
    if (it != transforms_.end()) {
        return it->second;
    }
    return identity_transform_;
}

void TransformManager::set_transform(const std::string& model_id, const Transform& transform) {
    transforms_[model_id] = transform;
    LOG_DEBUG("TransformManager: Set transform for model '{}'", model_id);
}

glm::mat4 TransformManager::get_model_matrix(const std::string& model_id) const {
    const Transform& transform = get_transform(model_id);
    return transform.get_model_matrix();
}

bool TransformManager::start_drag(float screen_x, float screen_y,
                                 float screen_width, float screen_height,
                                 const Camera& camera,
                                 const Scene& scene,
                                 CoroutineResourceManager& resource_manager) {
    if (is_dragging()) {
        LOG_WARN("TransformManager: Already dragging, ending previous drag");
        end_drag();
    }

    // Generate ray from screen coordinates
    Ray ray = RaycastUtils::screen_to_world_ray(screen_x, screen_y, 
                                               screen_width, screen_height, camera);

    // Create callback to get model transforms
    auto get_transform_callback = [this](const std::string& model_id) -> glm::mat4 {
        return this->get_model_matrix(model_id);
    };

    // Perform raycast to find object
    RaycastHit hit = RaycastUtils::raycast_scene(ray, scene, resource_manager, get_transform_callback);

    if (!hit.hit) {
        LOG_DEBUG("TransformManager: No object hit at screen ({}, {})", screen_x, screen_y);
        return false;
    }

    // Initialize drag info
    drag_info_.model_id = hit.model_id;
    drag_info_.initial_hit_point = hit.point;
    drag_info_.mode = current_mode_;
    drag_info_.state = DragState::kDragging;

    // Get transform for the model and calculate offset
    Transform& transform = get_transform(hit.model_id);
    drag_info_.drag_offset = hit.point - transform.get_position();

    LOG_INFO("TransformManager: Started dragging model '{}' at ({:.2f}, {:.2f}, {:.2f})",
             hit.model_id, hit.point.x, hit.point.y, hit.point.z);

    return true;
}

bool TransformManager::update_drag(float screen_x, float screen_y,
                                  float screen_width, float screen_height,
                                  const Camera& camera) {
    if (!is_dragging()) {
        return false;
    }

    // Calculate new world position based on screen movement
    glm::vec3 new_world_pos = calculate_drag_world_position(screen_x, screen_y,
                                                           screen_width, screen_height, camera);

    // Update model transform based on mode
    Transform& transform = get_transform(drag_info_.model_id);
    
    switch (drag_info_.mode) {
        case TransformMode::kTranslate: {
            glm::vec3 new_model_pos = new_world_pos - drag_info_.drag_offset;
            transform.set_position(new_model_pos);
            break;
        }
        case TransformMode::kRotate:
            LOG_DEBUG("TransformManager: Rotation mode not yet implemented");
            break;
        case TransformMode::kScale:
            LOG_DEBUG("TransformManager: Scale mode not yet implemented");
            break;
    }

    LOG_DEBUG("TransformManager: Updated drag position to ({:.2f}, {:.2f}, {:.2f})",
              new_world_pos.x, new_world_pos.y, new_world_pos.z);

    return true;
}

void TransformManager::end_drag() {
    if (!is_dragging()) {
        return;
    }

    LOG_INFO("TransformManager: Ended drag for model '{}'", drag_info_.model_id);
    drag_info_.reset();
}

glm::vec3 TransformManager::calculate_drag_world_position(float screen_x, float screen_y,
                                                         float screen_width, float screen_height,
                                                         const Camera& camera) const {
    // Generate ray for current screen position
    Ray current_ray = RaycastUtils::screen_to_world_ray(screen_x, screen_y,
                                                       screen_width, screen_height, camera);
    
    // Calculate distance from camera to initial hit point
    float hit_distance = glm::length(drag_info_.initial_hit_point - camera.get_position());
    
    // Project the ray to the same distance as the initial hit
    glm::vec3 projected_point = current_ray.origin + current_ray.direction * hit_distance;
    
    return projected_point;
}
